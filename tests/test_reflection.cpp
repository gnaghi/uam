/*
 * Unit tests for the reflection sidecar (.refl) wire format.
 *
 * The reflection sidecar is the contract between two projects that cannot be
 * linked together in one host process:
 *   - WRITER: uam, DekoCompiler::OutputReflection() (source/compiler_iface.cpp)
 *   - READER: SwitchGLES, sgl_load_reflection_sidecar() (source/gl/gl_shader.c)
 *
 * Both sides agree only through the POD structs and constants in <uam.h>. This
 * test pins that contract: it writes a .refl exactly as OutputReflection does
 * (fixed struct layout, documented record order), reads it back with the same
 * logic the SwitchGLES parser uses, and asserts a lossless round-trip plus the
 * rejection/edge cases the reader must handle (bad magic, wrong version,
 * truncation, count overflow).
 *
 * Standalone — depends only on <uam.h> and libc. Buildable and runnable on the
 * host (no NV50_IR / Mesa). Mirror of the test_spirv_parser harness style.
 */

#include "uam.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// Micro test harness
// ============================================================================

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_BEGIN(name)                          \
   do {                                           \
      g_tests_run++;                              \
      printf("  %-52s ", name);                   \
   } while (0)

#define TEST_END()                                \
   do {                                           \
      g_tests_passed++;                           \
      printf("[PASS]\n");                         \
   } while (0)

#define ASSERT(cond)                                                       \
   do {                                                                    \
      if (!(cond)) {                                                       \
         printf("[FAIL]\n    assertion failed: %s\n    at %s:%d\n",        \
                #cond, __FILE__, __LINE__);                                \
         g_tests_failed++;                                                 \
         return;                                                           \
      }                                                                    \
   } while (0)

#define ASSERT_EQ(a, b)                                                    \
   do {                                                                    \
      long long _a = (long long)(a); long long _b = (long long)(b);       \
      if (_a != _b) {                                                      \
         printf("[FAIL]\n    %s == %s failed: %lld != %lld\n    at %s:%d\n",\
                #a, #b, _a, _b, __FILE__, __LINE__);                       \
         g_tests_failed++;                                                 \
         return;                                                           \
      }                                                                    \
   } while (0)

#define ASSERT_STR_EQ(a, b)                                                \
   do {                                                                    \
      if (std::string(a) != std::string(b)) {                              \
         printf("[FAIL]\n    %s == %s failed: \"%s\" != \"%s\"\n    at %s:%d\n",\
                #a, #b, (a), (b), __FILE__, __LINE__);                     \
         g_tests_failed++;                                                 \
         return;                                                           \
      }                                                                    \
   } while (0)

// ============================================================================
// Layout contract — these sizes are baked into the on-disk format. If a struct
// in <uam.h> changes size, the .refl produced by uam and read by SwitchGLES
// silently desync. Pin them here so such a change fails the build, not a user.
// ============================================================================

static_assert(sizeof(uam_refl_header_t)  == 40,  "refl header layout changed");
static_assert(sizeof(uam_refl_uniform_t) == 144, "refl uniform layout changed");
static_assert(sizeof(uam_refl_sampler_t) == 136, "refl sampler layout changed");
static_assert(sizeof(uam_refl_input_t)   == 136, "refl input layout changed");

// ============================================================================
// Writer — mirrors DekoCompiler::OutputReflection byte layout:
//   [header][uniforms][samplers][inputs][constbuf data]
// ============================================================================

struct ReflModel {
   uint32_t stage = 0;
   uint32_t constbuf_size = 0;
   int32_t  depth_range_offset = -1;
   uint32_t flags = 0;
   std::vector<uam_refl_uniform_t> uniforms;
   std::vector<uam_refl_sampler_t> samplers;
   std::vector<uam_refl_input_t>   inputs;
   std::vector<uint8_t>            constbuf;
};

static bool write_refl(const char* path, const ReflModel& m)
{
   FILE* f = fopen(path, "wb");
   if (!f)
      return false;

   uam_refl_header_t hdr;
   memset(&hdr, 0, sizeof(hdr));
   memcpy(hdr.magic, UAM_REFL_MAGIC, sizeof(hdr.magic));
   hdr.version            = UAM_REFL_VERSION;
   hdr.stage              = m.stage;
   hdr.num_uniforms       = (uint32_t)m.uniforms.size();
   hdr.num_samplers       = (uint32_t)m.samplers.size();
   hdr.num_inputs         = (uint32_t)m.inputs.size();
   hdr.constbuf_size      = m.constbuf_size;
   hdr.depth_range_offset = m.depth_range_offset;
   hdr.constbuf_data_size = (uint32_t)m.constbuf.size();
   hdr.flags              = m.flags;
   fwrite(&hdr, 1, sizeof(hdr), f);

   for (const auto& u : m.uniforms) fwrite(&u, 1, sizeof(u), f);
   for (const auto& s : m.samplers) fwrite(&s, 1, sizeof(s), f);
   for (const auto& i : m.inputs)   fwrite(&i, 1, sizeof(i), f);
   if (!m.constbuf.empty())
      fwrite(m.constbuf.data(), 1, m.constbuf.size(), f);

   fclose(f);
   return true;
}

static uam_refl_uniform_t make_uniform(const char* name, uint32_t off, uint32_t size,
                                       uint8_t base, uint8_t vec, uint8_t mat,
                                       uint8_t is_sampler, uint32_t array)
{
   uam_refl_uniform_t u;
   memset(&u, 0, sizeof(u));
   strncpy(u.name, name, UAM_REFL_MAX_NAME - 1);
   u.offset = off; u.size_bytes = size;
   u.base_type = base; u.vector_elements = vec; u.matrix_columns = mat;
   u.is_sampler = is_sampler; u.array_elements = array;
   return u;
}

static uam_refl_sampler_t make_sampler(const char* name, int32_t binding, uint8_t type)
{
   uam_refl_sampler_t s;
   memset(&s, 0, sizeof(s));
   strncpy(s.name, name, UAM_REFL_MAX_NAME - 1);
   s.binding = binding; s.type = type;
   return s;
}

static uam_refl_input_t make_input(const char* name, int32_t loc, uint8_t base,
                                   uint8_t vec, uint8_t mat)
{
   uam_refl_input_t i;
   memset(&i, 0, sizeof(i));
   strncpy(i.name, name, UAM_REFL_MAX_NAME - 1);
   i.location = loc; i.base_type = base; i.vector_elements = vec; i.matrix_columns = mat;
   return i;
}

// ============================================================================
// Reader — mirrors SwitchGLES sgl_load_reflection_sidecar() validation + parse.
// Caps mimic SGL_MESA_MAX_* (SwitchGLES drops overflow but keeps reading).
// Returns true on a valid sidecar, false on reject (bad magic/version/truncation).
// ============================================================================

#define R_MAX_UNIFORMS 64
#define R_MAX_SAMPLERS 16
#define R_MAX_INPUTS   32

struct ReadResult {
   uam_refl_header_t hdr;
   uam_refl_uniform_t uniforms[R_MAX_UNIFORMS]; int num_uniforms = 0;
   uam_refl_sampler_t samplers[R_MAX_SAMPLERS]; int num_samplers = 0;
   uam_refl_input_t   inputs[R_MAX_INPUTS];     int num_inputs = 0;
   std::vector<uint8_t> constbuf;
};

static bool read_refl(const char* path, ReadResult& out)
{
   FILE* f = fopen(path, "rb");
   if (!f)
      return false;

   if (fread(&out.hdr, 1, sizeof(out.hdr), f) != sizeof(out.hdr) ||
       memcmp(out.hdr.magic, UAM_REFL_MAGIC, sizeof(out.hdr.magic)) != 0 ||
       out.hdr.version != UAM_REFL_VERSION) {
      fclose(f);
      return false;
   }

   bool ok = true;
   for (uint32_t i = 0; i < out.hdr.num_uniforms && ok; i++) {
      uam_refl_uniform_t rec;
      if (fread(&rec, 1, sizeof(rec), f) != sizeof(rec)) { ok = false; break; }
      if (out.num_uniforms < R_MAX_UNIFORMS) out.uniforms[out.num_uniforms++] = rec;
   }
   for (uint32_t i = 0; i < out.hdr.num_samplers && ok; i++) {
      uam_refl_sampler_t rec;
      if (fread(&rec, 1, sizeof(rec), f) != sizeof(rec)) { ok = false; break; }
      if (out.num_samplers < R_MAX_SAMPLERS) out.samplers[out.num_samplers++] = rec;
   }
   for (uint32_t i = 0; i < out.hdr.num_inputs && ok; i++) {
      uam_refl_input_t rec;
      if (fread(&rec, 1, sizeof(rec), f) != sizeof(rec)) { ok = false; break; }
      if (out.num_inputs < R_MAX_INPUTS) out.inputs[out.num_inputs++] = rec;
   }
   if (ok && out.hdr.constbuf_data_size > 0) {
      out.constbuf.resize(out.hdr.constbuf_data_size);
      if (fread(out.constbuf.data(), 1, out.hdr.constbuf_data_size, f) !=
          out.hdr.constbuf_data_size)
         ok = false;
   }

   fclose(f);
   return ok;
}

// Mirror of SwitchGLES uam_base_type_to_gl (source/gl/gl_shader.c). Mesa base
// types: 0=uint, 1=int, 2=float, 11=bool, 12=sampler.  GL enums hard-coded so a
// drift in the SwitchGLES converter is caught against an independent reference.
enum { T_FLOAT=0x1406, T_FLOAT_VEC2=0x8B50, T_FLOAT_VEC3=0x8B51, T_FLOAT_VEC4=0x8B52,
       T_FLOAT_MAT2=0x8B5A, T_FLOAT_MAT3=0x8B5B, T_FLOAT_MAT4=0x8B5C,
       T_INT=0x1404, T_INT_VEC2=0x8B53, T_SAMPLER_2D=0x8B5E };

static unsigned base_to_gl(uint8_t base, uint8_t vec, uint8_t mat)
{
   if (base == 2) {
      if (mat == 2) return T_FLOAT_MAT2;
      if (mat == 3) return T_FLOAT_MAT3;
      if (mat == 4) return T_FLOAT_MAT4;
      if (vec == 2) return T_FLOAT_VEC2;
      if (vec == 3) return T_FLOAT_VEC3;
      if (vec == 4) return T_FLOAT_VEC4;
      return T_FLOAT;
   }
   if (base == 1) { return vec == 2 ? T_INT_VEC2 : T_INT; }
   if (base == 12) return T_SAMPLER_2D;
   return T_FLOAT_VEC4;
}

static const char* TMP_PATH = "test_reflection_tmp.refl";

// ============================================================================
// Tests
// ============================================================================

static void test_magic_and_version()
{
   TEST_BEGIN("header magic and version constants");
   ASSERT_EQ(UAM_REFL_VERSION, 1u);
   ASSERT_EQ(UAM_REFL_MAX_NAME, 128);
   ASSERT(memcmp(UAM_REFL_MAGIC, "SGLR", 4) == 0);
   TEST_END();
}

static void test_roundtrip_uniforms()
{
   TEST_BEGIN("round-trip uniforms (scalar, vec, mat, array)");
   ReflModel m;
   m.stage = 0; /* DkStage_Vertex */
   m.constbuf_size = 256;
   m.uniforms.push_back(make_uniform("u_mvp",   0,  64, 2, 4, 4, 0, 0)); /* mat4 */
   m.uniforms.push_back(make_uniform("u_color", 64, 16, 2, 4, 1, 0, 0)); /* vec4 */
   m.uniforms.push_back(make_uniform("u_arr",   80, 32, 2, 1, 1, 0, 8)); /* float[8] */
   ASSERT(write_refl(TMP_PATH, m));

   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r));
   ASSERT_EQ(r.hdr.num_uniforms, 3);
   ASSERT_EQ(r.hdr.stage, 0);
   ASSERT_EQ(r.hdr.constbuf_size, 256);
   ASSERT_EQ(r.num_uniforms, 3);

   ASSERT_STR_EQ(r.uniforms[0].name, "u_mvp");
   ASSERT_EQ(r.uniforms[0].offset, 0);
   ASSERT_EQ(r.uniforms[0].size_bytes, 64);
   ASSERT_EQ(base_to_gl(r.uniforms[0].base_type, r.uniforms[0].vector_elements,
                        r.uniforms[0].matrix_columns), (unsigned)T_FLOAT_MAT4);

   ASSERT_STR_EQ(r.uniforms[1].name, "u_color");
   ASSERT_EQ(base_to_gl(r.uniforms[1].base_type, r.uniforms[1].vector_elements,
                        r.uniforms[1].matrix_columns), (unsigned)T_FLOAT_VEC4);

   ASSERT_STR_EQ(r.uniforms[2].name, "u_arr");
   ASSERT_EQ(r.uniforms[2].array_elements, 8);
   ASSERT_EQ(r.uniforms[2].offset, 80);
   TEST_END();
}

static void test_roundtrip_samplers_inputs()
{
   TEST_BEGIN("round-trip samplers and vertex inputs");
   ReflModel m;
   m.stage = 4; /* DkStage_Fragment */
   m.samplers.push_back(make_sampler("s_tex",  0, 0)); /* sampler2D */
   m.samplers.push_back(make_sampler("s_cube", 1, 1)); /* samplerCube */
   m.inputs.push_back(make_input("a_position", 0, 2, 4, 1)); /* vec4 */
   m.inputs.push_back(make_input("a_texcoord", 1, 2, 2, 1)); /* vec2 */
   ASSERT(write_refl(TMP_PATH, m));

   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r));
   ASSERT_EQ(r.num_samplers, 2);
   ASSERT_EQ(r.num_inputs, 2);
   ASSERT_STR_EQ(r.samplers[0].name, "s_tex");
   ASSERT_EQ(r.samplers[0].binding, 0);
   ASSERT_EQ(r.samplers[0].type, 0);
   ASSERT_STR_EQ(r.samplers[1].name, "s_cube");
   ASSERT_EQ(r.samplers[1].type, 1);
   ASSERT_STR_EQ(r.inputs[0].name, "a_position");
   ASSERT_EQ(r.inputs[0].location, 0);
   ASSERT_EQ(r.inputs[1].location, 1);
   ASSERT_EQ(base_to_gl(r.inputs[1].base_type, r.inputs[1].vector_elements,
                        r.inputs[1].matrix_columns), (unsigned)T_FLOAT_VEC2);
   TEST_END();
}

static void test_roundtrip_constbuf_and_depthrange()
{
   TEST_BEGIN("round-trip constbuf data and depth-range offset");
   ReflModel m;
   m.constbuf_size = 32;
   m.depth_range_offset = 16;
   m.flags = UAM_REFL_FLAG_CONSTBUF_REMAPPED;
   m.constbuf.resize(32);
   for (int i = 0; i < 32; i++) m.constbuf[i] = (uint8_t)(i * 7 + 1);
   ASSERT(write_refl(TMP_PATH, m));

   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r));
   ASSERT_EQ(r.hdr.depth_range_offset, 16);
   ASSERT_EQ(r.hdr.flags & UAM_REFL_FLAG_CONSTBUF_REMAPPED, UAM_REFL_FLAG_CONSTBUF_REMAPPED);
   ASSERT_EQ(r.hdr.constbuf_data_size, 32);
   ASSERT_EQ((int)r.constbuf.size(), 32);
   for (int i = 0; i < 32; i++)
      ASSERT_EQ(r.constbuf[i], (uint8_t)(i * 7 + 1));
   TEST_END();
}

static void test_empty_reflection()
{
   TEST_BEGIN("empty reflection (no uniforms/samplers/inputs)");
   ReflModel m;
   ASSERT(write_refl(TMP_PATH, m));
   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r));
   ASSERT_EQ(r.num_uniforms, 0);
   ASSERT_EQ(r.num_samplers, 0);
   ASSERT_EQ(r.num_inputs, 0);
   ASSERT_EQ(r.hdr.depth_range_offset, -1);
   TEST_END();
}

static void test_reject_bad_magic()
{
   TEST_BEGIN("reject file with bad magic");
   ReflModel m;
   m.uniforms.push_back(make_uniform("u", 0, 4, 2, 1, 1, 0, 0));
   ASSERT(write_refl(TMP_PATH, m));
   /* Corrupt the magic in place */
   FILE* f = fopen(TMP_PATH, "r+b");
   ASSERT(f != nullptr);
   const char bad[4] = { 'X', 'X', 'X', 'X' };
   fwrite(bad, 1, 4, f);
   fclose(f);

   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r) == false);
   TEST_END();
}

static void test_reject_bad_version()
{
   TEST_BEGIN("reject file with unsupported version");
   ReflModel m;
   ASSERT(write_refl(TMP_PATH, m));
   /* Patch version field (offset 4) to 99 */
   FILE* f = fopen(TMP_PATH, "r+b");
   ASSERT(f != nullptr);
   fseek(f, 4, SEEK_SET);
   uint32_t v = 99;
   fwrite(&v, 1, sizeof(v), f);
   fclose(f);

   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r) == false);
   TEST_END();
}

static void test_reject_truncated_records()
{
   TEST_BEGIN("reject truncated record stream");
   /* Header claims 3 uniforms but only 1 record is present. */
   FILE* f = fopen(TMP_PATH, "wb");
   ASSERT(f != nullptr);
   uam_refl_header_t hdr;
   memset(&hdr, 0, sizeof(hdr));
   memcpy(hdr.magic, UAM_REFL_MAGIC, 4);
   hdr.version = UAM_REFL_VERSION;
   hdr.num_uniforms = 3;
   fwrite(&hdr, 1, sizeof(hdr), f);
   uam_refl_uniform_t u = make_uniform("only_one", 0, 4, 2, 1, 1, 0, 0);
   fwrite(&u, 1, sizeof(u), f);
   fclose(f);

   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r) == false);
   TEST_END();
}

static void test_overflow_caps_kept_reading()
{
   TEST_BEGIN("count over reader cap: overflow dropped, rest parsed");
   ReflModel m;
   /* More uniforms than the reader cap; all records present in the file. */
   for (int i = 0; i < R_MAX_UNIFORMS + 5; i++) {
      char nm[32];
      snprintf(nm, sizeof(nm), "u_%d", i);
      m.uniforms.push_back(make_uniform(nm, (uint32_t)(i * 4), 4, 2, 1, 1, 0, 0));
   }
   /* A sampler follows the uniform overflow — must still be reached. */
   m.samplers.push_back(make_sampler("s_after", 0, 0));
   ASSERT(write_refl(TMP_PATH, m));

   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r));               /* still a valid file */
   ASSERT_EQ(r.hdr.num_uniforms, R_MAX_UNIFORMS + 5);
   ASSERT_EQ(r.num_uniforms, R_MAX_UNIFORMS);    /* capped */
   ASSERT_EQ(r.num_samplers, 1);                 /* stayed file-aligned */
   ASSERT_STR_EQ(r.samplers[0].name, "s_after");
   TEST_END();
}

static void test_long_name_truncation()
{
   TEST_BEGIN("over-long name stays NUL-terminated within buffer");
   ReflModel m;
   std::string longname(UAM_REFL_MAX_NAME + 50, 'a');
   m.uniforms.push_back(make_uniform(longname.c_str(), 0, 4, 2, 1, 1, 0, 0));
   ASSERT(write_refl(TMP_PATH, m));
   ReadResult r;
   ASSERT(read_refl(TMP_PATH, r));
   ASSERT_EQ((int)strlen(r.uniforms[0].name), UAM_REFL_MAX_NAME - 1);
   TEST_END();
}

int main()
{
   printf("=== Reflection sidecar (.refl) format tests ===\n\n");

   printf("Constants:\n");
   test_magic_and_version();

   printf("\nRound-trip:\n");
   test_roundtrip_uniforms();
   test_roundtrip_samplers_inputs();
   test_roundtrip_constbuf_and_depthrange();
   test_empty_reflection();

   printf("\nRejection & edge cases:\n");
   test_reject_bad_magic();
   test_reject_bad_version();
   test_reject_truncated_records();
   test_overflow_caps_kept_reading();
   test_long_name_truncation();

   remove(TMP_PATH);

   printf("\n=== Results: %d/%d passed", g_tests_passed, g_tests_run);
   if (g_tests_failed)
      printf(", %d FAILED", g_tests_failed);
   printf(" ===\n");

   return g_tests_failed ? 1 : 0;
}

/*
 * Unit tests for the SPIR-V frontend parser (spirv_frontend.h/cpp).
 *
 * Each test builds a SPIR-V binary in-memory word-by-word, parses it with
 * spirv::program_create(), and verifies the resulting structures.
 *
 * Minimal test harness — no external dependency.
 */

#include "spirv_frontend.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

// ============================================================================
// Micro test harness
// ============================================================================

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static const char* g_current_test = "";

#define TEST_BEGIN(name)                          \
   do {                                           \
      g_current_test = name;                      \
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
      auto _a = (a); auto _b = (b);                                       \
      if (_a != _b) {                                                      \
         printf("[FAIL]\n    %s == %s failed: %u != %u\n    at %s:%d\n",   \
                #a, #b, (unsigned)_a, (unsigned)_b, __FILE__, __LINE__);   \
         g_tests_failed++;                                                 \
         return;                                                           \
      }                                                                    \
   } while (0)

#define ASSERT_STR_EQ(a, b)                                                \
   do {                                                                    \
      if (std::string(a) != std::string(b)) {                              \
         printf("[FAIL]\n    %s == %s failed: \"%s\" != \"%s\"\n"          \
                "    at %s:%d\n",                                          \
                #a, #b, (a), (b), __FILE__, __LINE__);                     \
         g_tests_failed++;                                                 \
         return;                                                           \
      }                                                                    \
   } while (0)

#define ASSERT_NOT_NULL(p)                                                 \
   do {                                                                    \
      if ((p) == nullptr) {                                                \
         printf("[FAIL]\n    %s is NULL\n    at %s:%d\n",                  \
                #p, __FILE__, __LINE__);                                   \
         g_tests_failed++;                                                 \
         return;                                                           \
      }                                                                    \
   } while (0)

#define ASSERT_NULL(p)                                                     \
   do {                                                                    \
      if ((p) != nullptr) {                                                \
         printf("[FAIL]\n    %s expected NULL\n    at %s:%d\n",            \
                #p, __FILE__, __LINE__);                                   \
         g_tests_failed++;                                                 \
         return;                                                           \
      }                                                                    \
   } while (0)

// ============================================================================
// SPIR-V binary builder helper
// ============================================================================

class SpvBuilder {
public:
   // Emit the 5-word header.  bound = max ID + 1.
   void header(uint32_t bound)
   {
      m_words.push_back(SpvMagicNumber);   // magic
      m_words.push_back(0x00010500);       // version 1.5
      m_words.push_back(0);                // generator
      m_words.push_back(bound);            // bound
      m_words.push_back(0);                // schema (reserved)
   }

   // Generic instruction: first word = (wordCount << 16) | opcode
   void inst(SpvOp op, std::initializer_list<uint32_t> operands)
   {
      uint32_t wc = 1 + (uint32_t)operands.size();
      m_words.push_back((wc << 16) | (uint32_t)op);
      for (uint32_t w : operands)
         m_words.push_back(w);
   }

   // Encode a null-terminated string into one or more 32-bit words.
   // Returns the words needed (including null-terminator padding).
   static std::vector<uint32_t> encodeString(const char* str)
   {
      size_t len = strlen(str) + 1;               // include NUL
      size_t wordCount = (len + 3) / 4;
      std::vector<uint32_t> words(wordCount, 0);
      memcpy(words.data(), str, len);
      return words;
   }

   // Instruction with an embedded string (e.g. OpName, OpEntryPoint)
   void instStr(SpvOp op, std::initializer_list<uint32_t> prefix,
                const char* str,
                std::initializer_list<uint32_t> suffix = {})
   {
      auto sw = encodeString(str);
      uint32_t wc = 1 + (uint32_t)prefix.size() + (uint32_t)sw.size()
                   + (uint32_t)suffix.size();
      m_words.push_back((wc << 16) | (uint32_t)op);
      for (uint32_t w : prefix) m_words.push_back(w);
      for (uint32_t w : sw)     m_words.push_back(w);
      for (uint32_t w : suffix) m_words.push_back(w);
   }

   const uint32_t* data() const { return m_words.data(); }
   size_t          size() const { return m_words.size(); }

private:
   std::vector<uint32_t> m_words;
};

// ============================================================================
// Tests
// ============================================================================

// --------------- 1. Header parsing -----------------------------------------

static void test_reject_bad_magic()
{
   TEST_BEGIN("reject bad magic");

   uint32_t words[] = { 0xDEADBEEF, 0x00010500, 0, 10, 0 };
   spirv::Program* p = spirv::program_create(words, 5, pipeline_stage_vertex);
   ASSERT_NULL(p);

   TEST_END();
}

static void test_reject_too_small()
{
   TEST_BEGIN("reject binary < 5 words");

   uint32_t words[] = { SpvMagicNumber, 0x00010500, 0, 10 };
   spirv::Program* p = spirv::program_create(words, 4, pipeline_stage_vertex);
   ASSERT_NULL(p);

   TEST_END();
}

static void test_header_fields()
{
   TEST_BEGIN("parse header fields (magic, version, bound)");

   SpvBuilder b;
   b.header(42);
   // Need at least a void type so the binary isn't empty
   b.inst(SpvOpTypeVoid, {1});

   spirv::Program* p = spirv::program_create(b.data(), b.size(),
                                              pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);
   ASSERT_EQ(p->magic, SpvMagicNumber);
   ASSERT_EQ(p->version, (uint32_t)0x00010500);
   ASSERT_EQ(p->bound, 42u);
   ASSERT_EQ(p->stage, pipeline_stage_vertex);
   delete p;

   TEST_END();
}

// --------------- 2. Scalar type parsing ------------------------------------

static void test_type_void()
{
   TEST_BEGIN("OpTypeVoid");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpTypeVoid, {1});   // %1 = void

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(1);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_VOID);
   ASSERT_EQ(ty->id, 1u);
   delete p;

   TEST_END();
}

static void test_type_bool()
{
   TEST_BEGIN("OpTypeBool");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpTypeBool, {1});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(1);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_BOOL);
   ASSERT_EQ(ty->bitWidth, 1u);
   delete p;

   TEST_END();
}

static void test_type_int()
{
   TEST_BEGIN("OpTypeInt (32-bit signed + 16-bit unsigned)");

   SpvBuilder b;
   b.header(3);
   b.inst(SpvOpTypeInt, {1, 32, 1});   // %1 = int32_t (signed)
   b.inst(SpvOpTypeInt, {2, 16, 0});   // %2 = uint16_t (unsigned)

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* t1 = p->getType(1);
   ASSERT_NOT_NULL(t1);
   ASSERT_EQ(t1->kind, spirv::TYPE_INT);
   ASSERT_EQ(t1->bitWidth, 32u);
   ASSERT_EQ(t1->signedness, 1u);

   auto* t2 = p->getType(2);
   ASSERT_NOT_NULL(t2);
   ASSERT_EQ(t2->kind, spirv::TYPE_INT);
   ASSERT_EQ(t2->bitWidth, 16u);
   ASSERT_EQ(t2->signedness, 0u);
   delete p;

   TEST_END();
}

static void test_type_float()
{
   TEST_BEGIN("OpTypeFloat (32 + 64 bit)");

   SpvBuilder b;
   b.header(3);
   b.inst(SpvOpTypeFloat, {1, 32});
   b.inst(SpvOpTypeFloat, {2, 64});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_EQ(p->getType(1)->kind, spirv::TYPE_FLOAT);
   ASSERT_EQ(p->getType(1)->bitWidth, 32u);
   ASSERT_EQ(p->getType(2)->bitWidth, 64u);
   delete p;

   TEST_END();
}

// --------------- 3. Composite type parsing ---------------------------------

static void test_type_vector()
{
   TEST_BEGIN("OpTypeVector (vec4 of float)");

   SpvBuilder b;
   b.header(3);
   b.inst(SpvOpTypeFloat, {1, 32});       // %1 = float
   b.inst(SpvOpTypeVector, {2, 1, 4});    // %2 = vec4

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(2);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_VECTOR);
   ASSERT_EQ(ty->elementTypeId, 1u);
   ASSERT_EQ(ty->componentCount, 4u);
   delete p;

   TEST_END();
}

static void test_type_matrix()
{
   TEST_BEGIN("OpTypeMatrix (mat4 = 4 columns of vec4)");

   SpvBuilder b;
   b.header(4);
   b.inst(SpvOpTypeFloat, {1, 32});
   b.inst(SpvOpTypeVector, {2, 1, 4});
   b.inst(SpvOpTypeMatrix, {3, 2, 4});    // %3 = mat4

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(3);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_MATRIX);
   ASSERT_EQ(ty->elementTypeId, 2u);
   ASSERT_EQ(ty->componentCount, 4u);
   delete p;

   TEST_END();
}

static void test_type_array()
{
   TEST_BEGIN("OpTypeArray (float[10])");

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeFloat, {1, 32});          // %1 = float
   b.inst(SpvOpTypeInt,   {2, 32, 0});       // %2 = uint
   b.inst(SpvOpConstant,  {2, 3, 10});       // %3 = 10u
   b.inst(SpvOpTypeArray,  {4, 1, 3});       // %4 = float[10]

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(4);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_ARRAY);
   ASSERT_EQ(ty->elementTypeId, 1u);
   ASSERT_EQ(ty->lengthId, 3u);

   // Verify getArrayLength goes through constant lookup
   ASSERT_EQ(p->getArrayLength(4), 10u);
   delete p;

   TEST_END();
}

static void test_type_struct()
{
   TEST_BEGIN("OpTypeStruct ({float, vec4, int})");

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeFloat,  {1, 32});
   b.inst(SpvOpTypeVector, {2, 1, 4});
   b.inst(SpvOpTypeInt,    {3, 32, 1});
   b.inst(SpvOpTypeStruct, {4, 1, 2, 3});  // %4 = struct{float,vec4,int}

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(4);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_STRUCT);
   ASSERT_EQ(ty->memberTypeIds.size(), 3u);
   ASSERT_EQ(ty->memberTypeIds[0], 1u);
   ASSERT_EQ(ty->memberTypeIds[1], 2u);
   ASSERT_EQ(ty->memberTypeIds[2], 3u);
   delete p;

   TEST_END();
}

static void test_type_pointer()
{
   TEST_BEGIN("OpTypePointer (Input ptr to vec4)");

   SpvBuilder b;
   b.header(4);
   b.inst(SpvOpTypeFloat,   {1, 32});
   b.inst(SpvOpTypeVector,  {2, 1, 4});
   b.inst(SpvOpTypePointer, {3, SpvStorageClassInput, 2});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(3);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_POINTER);
   ASSERT_EQ(ty->storageClass, SpvStorageClassInput);
   ASSERT_EQ(ty->elementTypeId, 2u);

   // getPointeeType
   auto* pointee = p->getPointeeType(3);
   ASSERT_NOT_NULL(pointee);
   ASSERT_EQ(pointee->kind, spirv::TYPE_VECTOR);
   ASSERT_EQ(pointee->componentCount, 4u);
   delete p;

   TEST_END();
}

static void test_type_function()
{
   TEST_BEGIN("OpTypeFunction (void(float, int))");

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeVoid,     {1});
   b.inst(SpvOpTypeFloat,    {2, 32});
   b.inst(SpvOpTypeInt,      {3, 32, 1});
   b.inst(SpvOpTypeFunction, {4, 1, 2, 3}); // void(float, int)

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ty = p->getType(4);
   ASSERT_NOT_NULL(ty);
   ASSERT_EQ(ty->kind, spirv::TYPE_FUNCTION);
   ASSERT_EQ(ty->returnTypeId, 1u);
   ASSERT_EQ(ty->paramTypeIds.size(), 2u);
   ASSERT_EQ(ty->paramTypeIds[0], 2u);
   ASSERT_EQ(ty->paramTypeIds[1], 3u);
   delete p;

   TEST_END();
}

// --------------- 4. Decoration parsing -------------------------------------

static void test_decoration_location()
{
   TEST_BEGIN("OpDecorate Location");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpDecorate, {1, SpvDecorationLocation, 7});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto& dec = p->getDecoration(1);
   ASSERT_EQ(dec.location, 7);
   // others should be default
   ASSERT_EQ(dec.binding, -1);
   ASSERT(dec.hasBuiltIn == false);
   delete p;

   TEST_END();
}

static void test_decoration_binding_set()
{
   TEST_BEGIN("OpDecorate Binding + DescriptorSet");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpDecorate, {1, SpvDecorationBinding, 3});
   b.inst(SpvOpDecorate, {1, SpvDecorationDescriptorSet, 0});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto& dec = p->getDecoration(1);
   ASSERT_EQ(dec.binding, 3);
   ASSERT_EQ(dec.descriptorSet, 0);
   delete p;

   TEST_END();
}

static void test_decoration_builtin()
{
   TEST_BEGIN("OpDecorate BuiltIn Position");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpDecorate, {1, SpvDecorationBuiltIn, SpvBuiltInPosition});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto& dec = p->getDecoration(1);
   ASSERT(dec.hasBuiltIn == true);
   ASSERT_EQ(dec.builtIn, SpvBuiltInPosition);
   delete p;

   TEST_END();
}

static void test_decoration_interpolation_flags()
{
   TEST_BEGIN("OpDecorate Flat + NoPerspective + Centroid");

   SpvBuilder b;
   b.header(4);
   b.inst(SpvOpDecorate, {1, SpvDecorationFlat});
   b.inst(SpvOpDecorate, {2, SpvDecorationNoPerspective});
   b.inst(SpvOpDecorate, {3, SpvDecorationCentroid});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT(p->getDecoration(1).flat == true);
   ASSERT(p->getDecoration(1).noPerspective == false);

   ASSERT(p->getDecoration(2).noPerspective == true);
   ASSERT(p->getDecoration(2).flat == false);

   ASSERT(p->getDecoration(3).centroid == true);
   delete p;

   TEST_END();
}

static void test_decoration_block()
{
   TEST_BEGIN("OpDecorate Block");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpDecorate, {1, SpvDecorationBlock});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT(p->getDecoration(1).block == true);
   ASSERT(p->getDecoration(1).bufferBlock == false);
   delete p;

   TEST_END();
}

static void test_member_decoration()
{
   TEST_BEGIN("OpMemberDecorate Offset + BuiltIn");

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeFloat,  {1, 32});
   b.inst(SpvOpTypeVector, {2, 1, 4});
   b.inst(SpvOpTypeStruct, {3, 2, 1});          // %3 = struct{vec4, float}
   b.inst(SpvOpMemberDecorate, {3, 0, SpvDecorationOffset, 0});
   b.inst(SpvOpMemberDecorate, {3, 0, SpvDecorationBuiltIn, SpvBuiltInPosition});
   b.inst(SpvOpMemberDecorate, {3, 1, SpvDecorationOffset, 16});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto& m0 = p->getMemberDecoration(3, 0);
   ASSERT_EQ(m0.offset, 0);
   ASSERT(m0.hasBuiltIn == true);
   ASSERT_EQ(m0.builtIn, SpvBuiltInPosition);

   auto& m1 = p->getMemberDecoration(3, 1);
   ASSERT_EQ(m1.offset, 16);
   ASSERT(m1.hasBuiltIn == false);
   delete p;

   TEST_END();
}

// --------------- 5. Constant parsing ---------------------------------------

static void test_constant_scalar()
{
   TEST_BEGIN("OpConstant (uint = 42, float = 3.14)");

   float pi = 3.14f;
   uint32_t pi_bits;
   memcpy(&pi_bits, &pi, 4);

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeInt,   {1, 32, 0});
   b.inst(SpvOpTypeFloat, {2, 32});
   b.inst(SpvOpConstant,  {1, 3, 42});       // %3 = 42u
   b.inst(SpvOpConstant,  {2, 4, pi_bits});   // %4 = 3.14f

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* c3 = p->getConstant(3);
   ASSERT_NOT_NULL(c3);
   ASSERT_EQ(c3->typeId, 1u);
   ASSERT_EQ(c3->value.u32, 42u);
   ASSERT(c3->isComposite == false);

   auto* c4 = p->getConstant(4);
   ASSERT_NOT_NULL(c4);
   ASSERT_EQ(c4->typeId, 2u);
   ASSERT(fabsf(c4->value.f32 - 3.14f) < 0.001f);
   delete p;

   TEST_END();
}

static void test_constant_true_false()
{
   TEST_BEGIN("OpConstantTrue / OpConstantFalse");

   SpvBuilder b;
   b.header(4);
   b.inst(SpvOpTypeBool,      {1});
   b.inst(SpvOpConstantTrue,  {1, 2});
   b.inst(SpvOpConstantFalse, {1, 3});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* ct = p->getConstant(2);
   ASSERT_NOT_NULL(ct);
   ASSERT(ct->isTrue == true);
   ASSERT_EQ(ct->value.u32, 1u);

   auto* cf = p->getConstant(3);
   ASSERT_NOT_NULL(cf);
   ASSERT(cf->isFalse == true);
   ASSERT_EQ(cf->value.u32, 0u);
   delete p;

   TEST_END();
}

static void test_constant_composite()
{
   TEST_BEGIN("OpConstantComposite (vec3 of floats)");

   float v0 = 1.0f, v1 = 2.0f, v2 = 3.0f;
   uint32_t b0, b1, b2;
   memcpy(&b0, &v0, 4); memcpy(&b1, &v1, 4); memcpy(&b2, &v2, 4);

   SpvBuilder b;
   b.header(8);
   b.inst(SpvOpTypeFloat,         {1, 32});
   b.inst(SpvOpTypeVector,        {2, 1, 3});
   b.inst(SpvOpConstant,          {1, 3, b0});   // 1.0
   b.inst(SpvOpConstant,          {1, 4, b1});   // 2.0
   b.inst(SpvOpConstant,          {1, 5, b2});   // 3.0
   b.inst(SpvOpConstantComposite, {2, 6, 3, 4, 5}); // vec3(1,2,3)

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* cc = p->getConstant(6);
   ASSERT_NOT_NULL(cc);
   ASSERT(cc->isComposite == true);
   ASSERT_EQ(cc->constituents.size(), 3u);
   ASSERT_EQ(cc->constituents[0], 3u);
   ASSERT_EQ(cc->constituents[1], 4u);
   ASSERT_EQ(cc->constituents[2], 5u);
   delete p;

   TEST_END();
}

static void test_constant_null()
{
   TEST_BEGIN("OpConstantNull");

   SpvBuilder b;
   b.header(3);
   b.inst(SpvOpTypeInt,      {1, 32, 0});
   b.inst(SpvOpConstantNull, {1, 2});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* cn = p->getConstant(2);
   ASSERT_NOT_NULL(cn);
   ASSERT(cn->isNull == true);
   ASSERT_EQ(cn->value.u32, 0u);
   delete p;

   TEST_END();
}

// --------------- 6. Variable parsing ---------------------------------------

static void test_variable_input()
{
   TEST_BEGIN("OpVariable (Input vec4)");

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeFloat,   {1, 32});
   b.inst(SpvOpTypeVector,  {2, 1, 4});
   b.inst(SpvOpTypePointer, {3, SpvStorageClassInput, 2});
   b.inst(SpvOpVariable,    {3, 4, SpvStorageClassInput});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* var = p->getVariable(4);
   ASSERT_NOT_NULL(var);
   ASSERT_EQ(var->id, 4u);
   ASSERT_EQ(var->typeId, 3u);
   ASSERT_EQ(var->storageClass, SpvStorageClassInput);
   ASSERT_EQ(var->initializerId, 0u);
   delete p;

   TEST_END();
}

static void test_variable_output()
{
   TEST_BEGIN("OpVariable (Output float)");

   SpvBuilder b;
   b.header(4);
   b.inst(SpvOpTypeFloat,   {1, 32});
   b.inst(SpvOpTypePointer, {2, SpvStorageClassOutput, 1});
   b.inst(SpvOpVariable,    {2, 3, SpvStorageClassOutput});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_fragment);
   ASSERT_NOT_NULL(p);

   auto* var = p->getVariable(3);
   ASSERT_NOT_NULL(var);
   ASSERT_EQ(var->storageClass, SpvStorageClassOutput);
   delete p;

   TEST_END();
}

static void test_variable_uniform()
{
   TEST_BEGIN("OpVariable (Uniform struct)");

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeFloat,   {1, 32});
   b.inst(SpvOpTypeStruct,  {2, 1, 1});   // struct{float, float}
   b.inst(SpvOpTypePointer, {3, SpvStorageClassUniform, 2});
   b.inst(SpvOpVariable,    {3, 4, SpvStorageClassUniform});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto* var = p->getVariable(4);
   ASSERT_NOT_NULL(var);
   ASSERT_EQ(var->storageClass, SpvStorageClassUniform);

   // Pointee is a struct
   auto* pointee = p->getPointeeType(var->typeId);
   ASSERT_NOT_NULL(pointee);
   ASSERT_EQ(pointee->kind, spirv::TYPE_STRUCT);
   delete p;

   TEST_END();
}

// --------------- 7. OpName ------------------------------------------------

static void test_opname()
{
   TEST_BEGIN("OpName (debug names)");

   SpvBuilder b;
   b.header(3);
   b.inst(SpvOpTypeFloat, {1, 32});
   b.instStr(SpvOpName, {1}, "myFloat");
   b.instStr(SpvOpName, {2}, "something_else");

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_STR_EQ(p->names[1].c_str(), "myFloat");
   ASSERT_STR_EQ(p->names[2].c_str(), "something_else");
   delete p;

   TEST_END();
}

// --------------- 8. EntryPoint + ExtInstImport -----------------------------

static void test_entry_point_vertex()
{
   TEST_BEGIN("OpEntryPoint (Vertex, with interfaces)");

   SpvBuilder b;
   b.header(20);

   // Types + variables that will be interface
   b.inst(SpvOpTypeFloat,   {1, 32});
   b.inst(SpvOpTypeVector,  {2, 1, 4});
   b.inst(SpvOpTypePointer, {3, SpvStorageClassInput, 2});
   b.inst(SpvOpTypePointer, {4, SpvStorageClassOutput, 2});
   b.inst(SpvOpVariable,    {3, 10, SpvStorageClassInput});
   b.inst(SpvOpVariable,    {4, 11, SpvStorageClassOutput});

   b.instStr(SpvOpEntryPoint,
             {SpvExecutionModelVertex, 5},  // function %5
             "main",
             {10, 11});                      // interfaces: %10, %11

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_EQ(p->entryPointId, 5u);
   ASSERT_STR_EQ(p->entryPointName.c_str(), "main");
   ASSERT_EQ(p->entryPointInterfaces.size(), 2u);
   ASSERT_EQ(p->entryPointInterfaces[0], 10u);
   ASSERT_EQ(p->entryPointInterfaces[1], 11u);
   delete p;

   TEST_END();
}

static void test_entry_point_wrong_stage()
{
   TEST_BEGIN("OpEntryPoint ignored when stage doesn't match");

   SpvBuilder b;
   b.header(10);
   b.instStr(SpvOpEntryPoint,
             {SpvExecutionModelFragment, 5},
             "main", {});

   // Parse as vertex — should NOT capture the fragment entry point
   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);
   ASSERT_EQ(p->entryPointId, 0u);   // not set
   delete p;

   TEST_END();
}

static void test_ext_inst_import()
{
   TEST_BEGIN("OpExtInstImport GLSL.std.450");

   SpvBuilder b;
   b.header(5);
   b.instStr(SpvOpExtInstImport, {1}, "GLSL.std.450");

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_EQ(p->extInstImports.size(), 1u);
   auto it = p->extInstImports.find(1);
   ASSERT(it != p->extInstImports.end());
   ASSERT_STR_EQ(it->second.c_str(), "GLSL.std.450");
   delete p;

   TEST_END();
}

// --------------- 9. Function + BasicBlock structure -------------------------

static void test_function_structure()
{
   TEST_BEGIN("OpFunction / OpLabel / OpReturn / OpFunctionEnd");

   SpvBuilder b;
   b.header(10);
   b.inst(SpvOpTypeVoid,     {1});
   b.inst(SpvOpTypeFunction, {2, 1});          // void()
   // function %3 : void()
   b.inst(SpvOpFunction, {1, 3, 0 /*None*/, 2});
   b.inst(SpvOpLabel,    {4});                  // block %4
   b.inst(SpvOpReturn,   {});
   b.inst(SpvOpFunctionEnd, {});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_EQ(p->functions.size(), 1u);
   auto& fn = p->functions[0];
   ASSERT_EQ(fn.id, 3u);
   ASSERT_EQ(fn.resultTypeId, 1u);
   ASSERT_EQ(fn.functionTypeId, 2u);
   ASSERT_EQ(fn.blocks.size(), 1u);
   ASSERT_EQ(fn.blocks[0].labelId, 4u);
   // Block should contain the OpReturn instruction
   ASSERT_EQ(fn.blocks[0].instructions.size(), 1u);
   ASSERT_EQ(fn.blocks[0].instructions[0]->opcode, SpvOpReturn);
   delete p;

   TEST_END();
}

static void test_function_multiple_blocks()
{
   TEST_BEGIN("function with 2 basic blocks");

   SpvBuilder b;
   b.header(10);
   b.inst(SpvOpTypeVoid,     {1});
   b.inst(SpvOpTypeFunction, {2, 1});
   b.inst(SpvOpFunction,     {1, 3, 0, 2});
   b.inst(SpvOpLabel,        {4});              // block A
   b.inst(SpvOpBranch,       {5});
   b.inst(SpvOpLabel,        {5});              // block B
   b.inst(SpvOpReturn,       {});
   b.inst(SpvOpFunctionEnd,  {});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_EQ(p->functions[0].blocks.size(), 2u);
   ASSERT_EQ(p->functions[0].blocks[0].labelId, 4u);
   ASSERT_EQ(p->functions[0].blocks[1].labelId, 5u);

   // Block A has one OpBranch instruction
   ASSERT_EQ(p->functions[0].blocks[0].instructions.size(), 1u);
   ASSERT_EQ(p->functions[0].blocks[0].instructions[0]->opcode, SpvOpBranch);

   // Block B has one OpReturn
   ASSERT_EQ(p->functions[0].blocks[1].instructions.size(), 1u);
   ASSERT_EQ(p->functions[0].blocks[1].instructions[0]->opcode, SpvOpReturn);
   delete p;

   TEST_END();
}

static void test_function_parameters()
{
   TEST_BEGIN("OpFunctionParameter");

   SpvBuilder b;
   b.header(10);
   b.inst(SpvOpTypeVoid,     {1});
   b.inst(SpvOpTypeFloat,    {2, 32});
   b.inst(SpvOpTypeFunction, {3, 1, 2, 2});    // void(float, float)
   b.inst(SpvOpFunction,          {1, 4, 0, 3});
   b.inst(SpvOpFunctionParameter, {2, 5});      // param %5 : float
   b.inst(SpvOpFunctionParameter, {2, 6});      // param %6 : float
   b.inst(SpvOpLabel,             {7});
   b.inst(SpvOpReturn,            {});
   b.inst(SpvOpFunctionEnd,       {});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_EQ(p->functions[0].paramIds.size(), 2u);
   ASSERT_EQ(p->functions[0].paramIds[0], 5u);
   ASSERT_EQ(p->functions[0].paramIds[1], 6u);
   delete p;

   TEST_END();
}

// --------------- 10. getTypeByteSize --------------------------------------

static void test_type_byte_size()
{
   TEST_BEGIN("getTypeByteSize (scalar, vector, array, struct)");

   SpvBuilder b;
   b.header(10);
   b.inst(SpvOpTypeFloat,  {1, 32});                // float = 4
   b.inst(SpvOpTypeInt,    {2, 32, 0});              // uint = 4
   b.inst(SpvOpTypeVector, {3, 1, 4});               // vec4 = 16
   b.inst(SpvOpConstant,   {2, 4, 5});               // %4 = 5u
   b.inst(SpvOpTypeArray,  {5, 1, 4});               // float[5] = 20
   b.inst(SpvOpTypeStruct, {6, 1, 3});               // struct{float,vec4}
   b.inst(SpvOpMemberDecorate, {6, 0, SpvDecorationOffset, 0});
   b.inst(SpvOpMemberDecorate, {6, 1, SpvDecorationOffset, 16});
   b.inst(SpvOpTypeFloat,  {7, 64});                 // double = 8
   b.inst(SpvOpTypeInt,    {8, 16, 1});              // int16 = 2

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_EQ(p->getTypeByteSize(1), 4u);    // float
   ASSERT_EQ(p->getTypeByteSize(2), 4u);    // uint
   ASSERT_EQ(p->getTypeByteSize(3), 16u);   // vec4
   ASSERT_EQ(p->getTypeByteSize(5), 20u);   // float[5]
   ASSERT_EQ(p->getTypeByteSize(6), 32u);   // struct{float@0, vec4@16} = 16+16
   ASSERT_EQ(p->getTypeByteSize(7), 8u);    // double
   ASSERT_EQ(p->getTypeByteSize(8), 2u);    // int16
   delete p;

   TEST_END();
}

// --------------- 11. Combined: realistic vertex shader module ---------------

static void test_realistic_vertex_module()
{
   TEST_BEGIN("realistic vertex passthrough module");

   //
   // Equivalent to:
   //   #version 450
   //   layout(location=0) in vec4 inPosition;
   //   layout(location=1) in vec4 inColor;
   //   layout(location=0) out vec4 outColor;
   //   void main() {
   //       gl_Position = inPosition;
   //       outColor = inColor;
   //   }
   //
   // IDs:
   //  1 = void
   //  2 = float
   //  3 = vec4
   //  4 = ptr<Input, vec4>
   //  5 = ptr<Output, vec4>
   //  6 = functype void()
   //  7 = struct{vec4} (gl_PerVertex)
   //  8 = ptr<Output, gl_PerVertex>
   // 10 = inPosition
   // 11 = inColor
   // 12 = outColor
   // 13 = gl_PerVertex
   // 14 = int
   // 15 = const 0
   // 16 = ptr<Output, vec4> (for access chain into gl_PerVertex)
   // 20 = main function
   // 21 = entry label

   SpvBuilder b;
   b.header(30);

   // Capability + MemoryModel
   b.inst(SpvOpCapability,  {1 /*Shader*/});
   b.inst(SpvOpMemoryModel, {0 /*Logical*/, 1 /*GLSL450*/});

   // ExtInstImport
   b.instStr(SpvOpExtInstImport, {9}, "GLSL.std.450");

   // Types
   b.inst(SpvOpTypeVoid,     {1});
   b.inst(SpvOpTypeFloat,    {2, 32});
   b.inst(SpvOpTypeVector,   {3, 2, 4});
   b.inst(SpvOpTypePointer,  {4, SpvStorageClassInput, 3});
   b.inst(SpvOpTypePointer,  {5, SpvStorageClassOutput, 3});
   b.inst(SpvOpTypeFunction, {6, 1});
   b.inst(SpvOpTypeStruct,   {7, 3});                          // gl_PerVertex
   b.inst(SpvOpTypePointer,  {8, SpvStorageClassOutput, 7});
   b.inst(SpvOpTypeInt,      {14, 32, 1});
   b.inst(SpvOpConstant,     {14, 15, 0});                     // int 0

   // Decorations
   b.inst(SpvOpDecorate, {10, SpvDecorationLocation, 0});      // inPosition @ 0
   b.inst(SpvOpDecorate, {11, SpvDecorationLocation, 1});      // inColor @ 1
   b.inst(SpvOpDecorate, {12, SpvDecorationLocation, 0});      // outColor @ 0
   b.inst(SpvOpDecorate, {7, SpvDecorationBlock});
   b.inst(SpvOpMemberDecorate, {7, 0, SpvDecorationBuiltIn, SpvBuiltInPosition});
   b.inst(SpvOpMemberDecorate, {7, 0, SpvDecorationOffset, 0});

   // Names
   b.instStr(SpvOpName, {10}, "inPosition");
   b.instStr(SpvOpName, {11}, "inColor");
   b.instStr(SpvOpName, {12}, "outColor");
   b.instStr(SpvOpName, {13}, "gl_PerVertex");

   // Variables
   b.inst(SpvOpVariable, {4, 10, SpvStorageClassInput});
   b.inst(SpvOpVariable, {4, 11, SpvStorageClassInput});
   b.inst(SpvOpVariable, {5, 12, SpvStorageClassOutput});
   b.inst(SpvOpVariable, {8, 13, SpvStorageClassOutput});

   // EntryPoint
   b.instStr(SpvOpEntryPoint,
             {SpvExecutionModelVertex, 20},
             "main",
             {10, 11, 12, 13});

   // Function
   b.inst(SpvOpFunction,    {1, 20, 0, 6});
   b.inst(SpvOpLabel,       {21});
   // %22 = load inPosition
   b.inst(SpvOpLoad,        {3, 22, 10});
   // %23 = access chain gl_PerVertex[0]
   b.inst(SpvOpAccessChain, {5, 23, 13, 15});
   // store gl_Position = inPosition
   b.inst(SpvOpStore,       {23, 22});
   // %24 = load inColor
   b.inst(SpvOpLoad,        {3, 24, 11});
   // store outColor = inColor
   b.inst(SpvOpStore,       {12, 24});
   b.inst(SpvOpReturn,      {});
   b.inst(SpvOpFunctionEnd, {});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   // --- Verify entry point ---
   ASSERT_EQ(p->entryPointId, 20u);
   ASSERT_STR_EQ(p->entryPointName.c_str(), "main");
   ASSERT_EQ(p->entryPointInterfaces.size(), 4u);

   // --- Verify types ---
   ASSERT_EQ(p->getType(1)->kind, spirv::TYPE_VOID);
   ASSERT_EQ(p->getType(2)->kind, spirv::TYPE_FLOAT);
   ASSERT_EQ(p->getType(3)->kind, spirv::TYPE_VECTOR);
   ASSERT_EQ(p->getType(3)->componentCount, 4u);
   ASSERT_EQ(p->getType(7)->kind, spirv::TYPE_STRUCT);
   ASSERT_EQ(p->getType(7)->memberTypeIds.size(), 1u);

   // --- Verify decorations ---
   ASSERT_EQ(p->getDecoration(10).location, 0);
   ASSERT_EQ(p->getDecoration(11).location, 1);
   ASSERT_EQ(p->getDecoration(12).location, 0);
   ASSERT(p->getDecoration(7).block == true);
   ASSERT(p->getMemberDecoration(7, 0).hasBuiltIn == true);
   ASSERT_EQ(p->getMemberDecoration(7, 0).builtIn, SpvBuiltInPosition);
   ASSERT_EQ(p->getMemberDecoration(7, 0).offset, 0);

   // --- Verify names ---
   ASSERT_STR_EQ(p->names[10].c_str(), "inPosition");
   ASSERT_STR_EQ(p->names[11].c_str(), "inColor");
   ASSERT_STR_EQ(p->names[12].c_str(), "outColor");

   // --- Verify variables ---
   auto* v10 = p->getVariable(10);
   ASSERT_NOT_NULL(v10);
   ASSERT_EQ(v10->storageClass, SpvStorageClassInput);
   ASSERT_EQ(v10->typeId, 4u);

   auto* v12 = p->getVariable(12);
   ASSERT_NOT_NULL(v12);
   ASSERT_EQ(v12->storageClass, SpvStorageClassOutput);

   auto* v13 = p->getVariable(13);
   ASSERT_NOT_NULL(v13);
   ASSERT_EQ(v13->storageClass, SpvStorageClassOutput);
   ASSERT_EQ(v13->typeId, 8u);

   // --- Verify constants ---
   auto* c15 = p->getConstant(15);
   ASSERT_NOT_NULL(c15);
   ASSERT_EQ(c15->value.i32, 0);

   // --- Verify function ---
   ASSERT_EQ(p->functions.size(), 1u);
   ASSERT_EQ(p->functions[0].id, 20u);
   ASSERT_EQ(p->functions[0].blocks.size(), 1u);

   auto& blk = p->functions[0].blocks[0];
   ASSERT_EQ(blk.labelId, 21u);
   // Should contain: Load, AccessChain, Store, Load, Store, Return = 6 instructions
   ASSERT_EQ(blk.instructions.size(), 6u);
   ASSERT_EQ(blk.instructions[0]->opcode, SpvOpLoad);
   ASSERT_EQ(blk.instructions[1]->opcode, SpvOpAccessChain);
   ASSERT_EQ(blk.instructions[2]->opcode, SpvOpStore);
   ASSERT_EQ(blk.instructions[3]->opcode, SpvOpLoad);
   ASSERT_EQ(blk.instructions[4]->opcode, SpvOpStore);
   ASSERT_EQ(blk.instructions[5]->opcode, SpvOpReturn);

   // --- Verify ext inst import ---
   ASSERT_EQ(p->extInstImports.size(), 1u);

   delete p;

   TEST_END();
}

// --------------- 12. Edge cases -------------------------------------------

static void test_unknown_ids_return_null()
{
   TEST_BEGIN("getType/getConstant/getVariable return null for unknown IDs");

   SpvBuilder b;
   b.header(5);
   b.inst(SpvOpTypeFloat, {1, 32});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   ASSERT_NULL(p->getType(0));
   ASSERT_NULL(p->getType(2));
   ASSERT_NULL(p->getConstant(1));
   ASSERT_NULL(p->getVariable(1));
   delete p;

   TEST_END();
}

static void test_multiple_decorations_same_id()
{
   TEST_BEGIN("multiple decorations on same ID accumulate");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpDecorate, {1, SpvDecorationLocation, 3});
   b.inst(SpvOpDecorate, {1, SpvDecorationFlat});
   b.inst(SpvOpDecorate, {1, SpvDecorationBinding, 2});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_vertex);
   ASSERT_NOT_NULL(p);

   auto& dec = p->getDecoration(1);
   ASSERT_EQ(dec.location, 3);
   ASSERT_EQ(dec.binding, 2);
   ASSERT(dec.flat == true);
   delete p;

   TEST_END();
}

static void test_stage_fragment()
{
   TEST_BEGIN("stage propagated to Program (fragment)");

   SpvBuilder b;
   b.header(2);
   b.inst(SpvOpTypeVoid, {1});

   auto* p = spirv::program_create(b.data(), b.size(), pipeline_stage_fragment);
   ASSERT_NOT_NULL(p);
   ASSERT_EQ(p->stage, pipeline_stage_fragment);
   delete p;

   TEST_END();
}

// ============================================================================
// Main
// ============================================================================

int main()
{
   printf("=== SPIR-V Parser Unit Tests ===\n\n");

   printf("Header:\n");
   test_reject_bad_magic();
   test_reject_too_small();
   test_header_fields();

   printf("\nScalar types:\n");
   test_type_void();
   test_type_bool();
   test_type_int();
   test_type_float();

   printf("\nComposite types:\n");
   test_type_vector();
   test_type_matrix();
   test_type_array();
   test_type_struct();
   test_type_pointer();
   test_type_function();

   printf("\nDecorations:\n");
   test_decoration_location();
   test_decoration_binding_set();
   test_decoration_builtin();
   test_decoration_interpolation_flags();
   test_decoration_block();
   test_member_decoration();

   printf("\nConstants:\n");
   test_constant_scalar();
   test_constant_true_false();
   test_constant_composite();
   test_constant_null();

   printf("\nVariables:\n");
   test_variable_input();
   test_variable_output();
   test_variable_uniform();

   printf("\nNames & imports:\n");
   test_opname();
   test_ext_inst_import();

   printf("\nEntry point:\n");
   test_entry_point_vertex();
   test_entry_point_wrong_stage();

   printf("\nFunctions:\n");
   test_function_structure();
   test_function_multiple_blocks();
   test_function_parameters();

   printf("\nSize computation:\n");
   test_type_byte_size();

   printf("\nIntegration:\n");
   test_realistic_vertex_module();

   printf("\nEdge cases:\n");
   test_unknown_ids_return_null();
   test_multiple_decorations_same_id();
   test_stage_fragment();

   printf("\n=== Results: %d/%d passed", g_tests_passed, g_tests_run);
   if (g_tests_failed)
      printf(", %d FAILED", g_tests_failed);
   printf(" ===\n");

   return g_tests_failed ? 1 : 0;
}

#pragma once
#include <stdint.h>

struct gl_shader_program;
struct tgsi_token;

typedef struct gl_shader_program* glsl_program;

enum pipeline_stage
{
	pipeline_stage_vertex,
	pipeline_stage_tess_ctrl,
	pipeline_stage_tess_eval,
	pipeline_stage_geometry,
	pipeline_stage_fragment,
	pipeline_stage_compute,
};

/* Uniform metadata for driver constbuf uniforms (bare uniforms in ES 1.00) */
#define GLSL_UNIFORM_MAX_NAME 128
#define GLSL_UNIFORM_MAX 128

typedef struct {
	char name[GLSL_UNIFORM_MAX_NAME];
	uint32_t offset;          /* Byte offset in driver constbuf */
	uint32_t size_bytes;      /* Total size in bytes */
	uint8_t  base_type;       /* GLSL_TYPE_FLOAT, GLSL_TYPE_INT, GLSL_TYPE_BOOL, GLSL_TYPE_SAMPLER */
	uint8_t  vector_elements; /* 1-4 */
	uint8_t  matrix_columns;  /* 1 for scalars/vectors, 2-4 for matrices */
	uint8_t  is_sampler;      /* 1 if sampler type */
	uint32_t array_elements;  /* 0 for non-array, N for array[N] */
} glsl_uniform_info_t;

void glsl_frontend_init();
void glsl_frontend_exit();

/* Error log capture - call reset before compilation, get_log after */
void glsl_frontend_reset_log();
const char* glsl_frontend_get_log();
void glsl_frontend_log(const char *fmt, ...);

/* Attribute binding (set before glsl_program_create for glBindAttribLocation support) */
#define GLSL_ATTRIB_BINDING_MAX 32

typedef struct {
	char name[GLSL_UNIFORM_MAX_NAME];
	int location;
} glsl_attrib_binding_t;

void glsl_frontend_set_attrib_bindings(const glsl_attrib_binding_t *bindings, int count);

/* Varying location bindings (set before glsl_program_create).
 * Each stage is linked as its own separable program, so Mesa lays out user
 * varyings from that stage's declarations alone: a fragment shader that
 * declares a different set of varyings than its vertex shader gets different
 * slots for the same name. Binding a name pins it to a generic slot, which is
 * applied as an explicit location to the VS outputs / FS inputs of that name.
 * Reuses glsl_attrib_binding_t (name + location). */
#define GLSL_VARYING_MAX 32

void glsl_frontend_set_varying_bindings(const glsl_attrib_binding_t *bindings, int count);

/* Runs only the Mesa preprocessor, with the same implicit #version and builtin
 * macros as glsl_program_create. Returns a malloc'd copy of the output, or NULL
 * on a preprocessor error. */
char* glsl_frontend_preprocess(const char* source, pipeline_stage stage);

glsl_program glsl_program_create(const char* source, pipeline_stage stage);

/* Links a vertex and a fragment shader into one (non-separable) program only
 * to run the cross-stage checks of the GLSL linker, which the per-stage
 * glsl_program_create cannot: matching varyings, uniforms and uniform blocks
 * declared in both stages. Generates no code. Returns NULL if the program
 * links, else a malloc'd copy of the link log. */
char* glsl_frontend_check_link(const char* vertex_source, const char* fragment_source);
const tgsi_token* glsl_program_get_tokens(glsl_program prg, unsigned int& num_tokens);
void* glsl_program_get_constant_buffer(glsl_program prg, unsigned int& out_size);
int8_t const* glsl_program_vertex_get_in_locations(glsl_program prg);
unsigned glsl_program_compute_get_shared_size(glsl_program prg);
void glsl_program_free(glsl_program prg);

/* Query uniform metadata from compiled program (driver constbuf uniforms) */
int glsl_program_get_num_uniforms(glsl_program prg);
const glsl_uniform_info_t* glsl_program_get_uniform_info(glsl_program prg, int index);
uint32_t glsl_program_get_constbuf_size(glsl_program prg);

/* Sampler metadata for ES 1.00 shaders */
#define GLSL_SAMPLER_MAX 16

typedef struct {
	char name[GLSL_UNIFORM_MAX_NAME];
	int binding;     /* Texture descriptor binding (auto-assigned 0, 1, 2...) */
	uint8_t type;    /* 0=sampler2D, 1=samplerCube */
} glsl_sampler_info_t;

int glsl_program_get_num_samplers(glsl_program prg);
const glsl_sampler_info_t* glsl_program_get_sampler_info(glsl_program prg, int index);

/* Vertex input (attribute) metadata — populated for vertex shaders */
#define GLSL_INPUT_MAX 32  /* up to 32 declared attributes (aliasing may exceed 16) */

typedef struct {
	char name[GLSL_UNIFORM_MAX_NAME];
	int location;        /* Generic attribute location (0-based, from layout or auto) */
	uint8_t base_type;   /* GLSL_TYPE_FLOAT, GLSL_TYPE_INT, GLSL_TYPE_BOOL */
	uint8_t vector_elements; /* 1-4 */
	uint8_t matrix_columns;  /* 1 for scalars/vectors, 2-4 for matrices */
	uint8_t pad;
} glsl_input_info_t;

int glsl_program_get_num_inputs(glsl_program prg);
const glsl_input_info_t* glsl_program_get_input_info(glsl_program prg, int index);

/* Returns byte offset of gl_DepthRange in driver constbuf, or -1 if not used.
 * The depth range occupies a vec4: [near, far, diff, unused]. */
int glsl_program_get_depth_range_offset(glsl_program prg);

/* User varying metadata — VS outputs or FS inputs, populated for those stages */
typedef struct {
	char name[GLSL_UNIFORM_MAX_NAME];
	int location;   /* Generic varying slot (0-based, VARYING_SLOT_VAR0 relative) */
	int num_slots;  /* Consecutive slots used (arrays/matrices use more than 1) */
} glsl_varying_info_t;

int glsl_program_get_num_varyings(glsl_program prg);
const glsl_varying_info_t* glsl_program_get_varying_info(glsl_program prg, int index);

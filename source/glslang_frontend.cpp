#include "glslang_frontend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glslang/Include/glslang_c_interface.h"
#include "glslang/Public/resource_limits_c.h"

static int s_glslang_refcount = 0;

void glslang_fe_init(void)
{
	if (s_glslang_refcount++ == 0)
		glslang_initialize_process();
}

void glslang_fe_exit(void)
{
	if (--s_glslang_refcount == 0)
		glslang_finalize_process();
}

static glslang_stage_t pipeline_to_glslang_stage(pipeline_stage stage)
{
	switch (stage) {
	case pipeline_stage_vertex:    return GLSLANG_STAGE_VERTEX;
	case pipeline_stage_tess_ctrl: return GLSLANG_STAGE_TESSCONTROL;
	case pipeline_stage_tess_eval: return GLSLANG_STAGE_TESSEVALUATION;
	case pipeline_stage_geometry:  return GLSLANG_STAGE_GEOMETRY;
	case pipeline_stage_fragment:  return GLSLANG_STAGE_FRAGMENT;
	case pipeline_stage_compute:   return GLSLANG_STAGE_COMPUTE;
	default:                       return GLSLANG_STAGE_VERTEX;
	}
}

bool glslang_fe_compile(const char *glsl_source, pipeline_stage stage,
                        uint32_t **out_words, size_t *out_word_count,
                        char **out_error)
{
	*out_words = NULL;
	*out_word_count = 0;
	if (out_error) *out_error = NULL;

	glslang_stage_t glslang_stage = pipeline_to_glslang_stage(stage);

	/* Set up input for GLSL 4.60 core → SPIR-V 1.0 (OpenGL target) */
	glslang_input_t input;
	memset(&input, 0, sizeof(input));
	input.language                = GLSLANG_SOURCE_GLSL;
	input.stage                   = glslang_stage;
	input.client                  = GLSLANG_CLIENT_OPENGL;
	input.client_version          = GLSLANG_TARGET_OPENGL_450;
	input.target_language         = GLSLANG_TARGET_SPV;
	input.target_language_version = GLSLANG_TARGET_SPV_1_0;
	input.code                    = glsl_source;
	input.default_version         = 460;
	input.default_profile         = GLSLANG_CORE_PROFILE;
	input.force_default_version_and_profile = 0;
	input.forward_compatible      = 0;
	input.messages                = (glslang_messages_t)(GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_SPV_RULES_BIT);
	input.resource                = glslang_default_resource();

	/* Create shader */
	glslang_shader_t *shader = glslang_shader_create(&input);
	if (!shader) {
		if (out_error)
			*out_error = strdup("Failed to create glslang shader object");
		return false;
	}

	/* Enable auto-mapping so uniforms without explicit layout qualifiers work */
	glslang_shader_set_options(shader, GLSLANG_SHADER_AUTO_MAP_BINDINGS |
	                                   GLSLANG_SHADER_AUTO_MAP_LOCATIONS);

	/* Preprocess (required: glslang_shader_parse uses preprocessedGLSL) */
	if (!glslang_shader_preprocess(shader, &input)) {
		const char *info = glslang_shader_get_info_log(shader);
		if (out_error && info)
			*out_error = strdup(info);
		else if (out_error)
			*out_error = strdup("glslang shader preprocess failed");
		glslang_shader_delete(shader);
		return false;
	}

	/* Parse (compile) */
	if (!glslang_shader_parse(shader, &input)) {
		const char *info = glslang_shader_get_info_log(shader);
		if (out_error && info)
			*out_error = strdup(info);
		else if (out_error)
			*out_error = strdup("glslang shader parse failed");
		glslang_shader_delete(shader);
		return false;
	}

	/* Create program and link */
	glslang_program_t *program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT)) {
		const char *info = glslang_program_get_info_log(program);
		if (out_error && info)
			*out_error = strdup(info);
		else if (out_error)
			*out_error = strdup("glslang program link failed");
		glslang_program_delete(program);
		glslang_shader_delete(shader);
		return false;
	}

	/* Generate SPIR-V */
	glslang_spv_options_t spv_options;
	memset(&spv_options, 0, sizeof(spv_options));
	spv_options.disable_optimizer = false;
	spv_options.validate          = false;

	glslang_program_SPIRV_generate_with_options(program, glslang_stage, &spv_options);

	size_t spv_size = glslang_program_SPIRV_get_size(program);
	if (spv_size == 0) {
		const char *msg = glslang_program_SPIRV_get_messages(program);
		if (out_error && msg)
			*out_error = strdup(msg);
		else if (out_error)
			*out_error = strdup("glslang SPIR-V generation produced no output");
		glslang_program_delete(program);
		glslang_shader_delete(shader);
		return false;
	}

	/* Copy SPIR-V to caller-owned buffer */
	uint32_t *words = (uint32_t *)malloc(spv_size * sizeof(uint32_t));
	if (!words) {
		if (out_error)
			*out_error = strdup("Out of memory allocating SPIR-V buffer");
		glslang_program_delete(program);
		glslang_shader_delete(shader);
		return false;
	}
	glslang_program_SPIRV_get(program, words);

	*out_words = words;
	*out_word_count = spv_size;

	/* Cleanup */
	glslang_program_delete(program);
	glslang_shader_delete(shader);
	return true;
}

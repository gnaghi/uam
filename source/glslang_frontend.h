#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "glsl_frontend.h" // for pipeline_stage enum

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize/finalize glslang process (call once) */
void glslang_fe_init(void);
void glslang_fe_exit(void);

/* Compile GLSL 4.60 source to SPIR-V binary.
 * Returns true on success. On success, *out_words and *out_word_count
 * are set (caller must free *out_words with free()).
 * On failure, returns false and sets *out_error (caller must free). */
bool glslang_fe_compile(const char *glsl_source, pipeline_stage stage,
                        uint32_t **out_words, size_t *out_word_count,
                        char **out_error);

#ifdef __cplusplus
}
#endif

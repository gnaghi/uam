# uam - deko3d shader compiler

uam compiles **GLSL 4.60** and **SPIR-V** shaders into DKSH (deko3d shader) binaries. It can be built as a **static library** for runtime compilation on Nintendo Switch, as a **CLI executable** for offline compilation on PC, or **both**. It targets the Nvidia Tegra X1 (Maxwell GM20B) GPU.

Based on [mesa](https://www.mesa3d.org/) 19.0.8's GLSL parser and TGSI infrastructure, and nouveau's nv50_ir code generation backend. The SPIR-V path bypasses TGSI and translates directly to NV50_IR.

## Building

### Prerequisites

- [devkitPro](https://devkitpro.org/) with devkitA64
- Python 3 with the `mako` module (`pacman -S python-mako`)
- `bison` and `flex` (`pacman -S bison flex`)
- `meson` and `ninja` (`pacman -S meson`)

### Build modes

The project can produce a **static library**, a **CLI executable**, or **both**, controlled by the `build_mode` option:

| Mode | Output | Use case |
|------|--------|----------|
| `lib` | `libuam.a` (static library) | Runtime shader compilation on Switch |
| `exe` | `uam` executable | Offline shader compilation on PC |
| `both` (default) | Both targets | Single project, no code duplication |

### Cross-compilation (library for Switch)

```bash
meson setup builddir --cross-file cross_switch.txt -Dbuild_mode=lib
meson compile -C builddir
DESTDIR=/opt/devkitpro/portlibs/switch meson install -C builddir
```

### Native build (CLI executable for PC)

```bash
meson setup builddir_host -Dbuild_mode=exe
meson compile -C builddir_host
```

Usage:

```bash
# GLSL input
uam -s vert -o output.dksh input.glsl

# SPIR-V input (auto-detected from magic number, or explicit)
uam -s vert -o output.dksh shader.spv
uam -s frag -i spirv -o output.dksh shader.spv
```

### Both targets

```bash
meson setup builddir -Dbuild_mode=both
meson compile -C builddir
```

## C API

```c
#include <uam/uam.h>

// Create a compiler for a specific shader stage
uam_compiler *compiler = uam_create_compiler(DkStage_Vertex);
// Or with a custom optimization level (0-3, default is 3):
uam_compiler *compiler = uam_create_compiler_ex(DkStage_Fragment, 2);

// Compile GLSL source to DKSH
const char *glsl = "#version 460\nlayout(location=0) in vec4 pos;\nvoid main() { gl_Position = pos; }";
if (uam_compile_dksh(compiler, glsl)) {
    // Get DKSH size and write to memory
    size_t size = uam_get_code_size(compiler);
    void *dksh = malloc(size);
    uam_write_code(compiler, dksh);

    // Query shader info
    int gprs = uam_get_num_gprs(compiler);
    unsigned int code_size = uam_get_raw_code_size(compiler);
} else {
    // Get error/warning log
    const char *log = uam_get_error_log(compiler);
    fprintf(stderr, "Compilation failed:\n%s\n", log);
}

// --- Attribute bindings (for GLES2 aliased attributes) ---
uam_set_attrib_binding(compiler, "a_position", 0);
uam_set_attrib_binding(compiler, "a_color", 1);

// --- ES 1.00 shader metadata (after successful compilation) ---
int num_uniforms = uam_get_num_uniforms(compiler);
for (int i = 0; i < num_uniforms; i++) {
    uam_uniform_info_t info;
    uam_get_uniform_info(compiler, i, &info);
    // info.name, info.offset, info.size_bytes, info.base_type, etc.
}
int num_samplers = uam_get_num_samplers(compiler);
bool remapped = uam_is_constbuf_remapped(compiler);
int dr_offset = uam_get_depth_range_offset(compiler); // -1 if not used

// Cleanup
uam_free_compiler(compiler);

// --- SPIR-V compilation ---
if (uam_compile_spirv(compiler, spirv_data, spirv_size)) {
    size_t size = uam_get_code_size(compiler);
    void *dksh = malloc(size);
    uam_write_code(compiler, dksh);
}
```

### Function reference

| Function | Description |
|----------|-------------|
| `uam_create_compiler(stage)` | Create compiler for a pipeline stage (opt level 3) |
| `uam_create_compiler_ex(stage, opt_level)` | Create compiler with custom optimization level |
| `uam_free_compiler(compiler)` | Destroy compiler and free resources |
| `uam_compile_dksh(compiler, glsl)` | Compile GLSL source, returns true on success |
| `uam_compile_spirv(compiler, data, size)` | Compile SPIR-V binary, returns true on success |
| `uam_get_code_size(compiler)` | Get DKSH binary size (with container) |
| `uam_get_raw_code_size(compiler)` | Get raw Maxwell bytecode size |
| `uam_write_code(compiler, memory)` | Write DKSH binary to a memory buffer |
| `uam_get_error_log(compiler)` | Get error/warning log from last compilation |
| `uam_get_num_gprs(compiler)` | Get GPU register count used by compiled shader |
| `uam_get_version(major, minor, micro)` | Get library version |
| `uam_set_attrib_binding(compiler, name, loc)` | Set attribute location binding before compilation |
| `uam_get_num_uniforms(compiler)` | Get number of driver constbuf uniforms (ES 1.00) |
| `uam_get_uniform_info(compiler, index, info)` | Get uniform metadata (name, offset, type, size) |
| `uam_get_num_samplers(compiler)` | Get number of samplers (ES 1.00) |
| `uam_get_sampler_info(compiler, index, info)` | Get sampler metadata (name, binding) |
| `uam_is_constbuf_remapped(compiler)` | True if driver constbuf remapped c[0]→c[1] |
| `uam_get_depth_range_offset(compiler)` | Byte offset of gl_DepthRange in constbuf (-1 if unused) |
| `uam_get_num_inputs(compiler)` | Get number of vertex shader inputs |
| `uam_get_input_info(compiler, index, info)` | Get input attribute metadata (name, location) |

## GLSL requirements

- **GLSL 4.60** (`#version 460`): UBO/SSBO/sampler bindings must be explicit (`layout(binding = N)`), `layout(location = N)` required for vertex inputs and varying I/O
- **GLSL ES 1.00** (`#version 100` or no `#version`): fully supported via Mesa direct compilation. Bare uniforms are mapped to the driver constbuf with metadata accessible via `uam_get_num_uniforms()` / `uam_get_uniform_info()`. Samplers are auto-bound sequentially. When no `#version` is present, `#version 100` is prepended automatically with `#line 1` to preserve `__LINE__` numbering.
- The `DEKO3D` preprocessor symbol is defined (value 100)

## SPIR-V support

uam accepts SPIR-V binaries as input (auto-detected from the `0x07230203` magic number, or forced with `-i spirv`). The SPIR-V path translates directly to NV50_IR, bypassing TGSI.

**Supported:** vertex and fragment shaders, arithmetic/bitwise/comparison/conversion ops, composites, memory access (load/store/access chain), control flow (branch/phi/kill), UBOs, built-in variables (gl_Position, gl_FragCoord, gl_VertexID, gl_InstanceID, gl_FrontFacing, gl_FragDepth), GLSL.std.450 math functions, interpolation qualifiers (flat, noPerspective, centroid).

**Not yet supported:** texture sampling (stubbed), SSBO/image access from SPIR-V, geometry/tessellation/compute stages (untested).

See [docs/SPIRV_SUPPORT.md](docs/SPIRV_SUPPORT.md) for full details.

## Differences with standard GL and mesa/nouveau

- UBO, SSBO, sampler and image bindings are **required to be explicit** (i.e. `layout (binding = N)`), and they have a one-to-one correspondence with deko3d bindings. Failure to specify explicit bindings will result in an error.
- There is support for 16 UBOs, 16 SSBOs, 32 "samplers" (combined image+sampler handle), and 8 images for each and every shader stage; with binding IDs ranging from zero to the corresponding limit minus one. However note that due to hardware limitations, only compute stage UBO bindings 0-5 are natively supported, while 6-15 are emulated as "SSBOs".
- Default uniforms outside UBO blocks are fully supported for **ES 1.00 shaders** (Mesa maps them to the driver constbuf; metadata is exposed via `uam_get_num_uniforms()`). For **GLSL 4.60**, default uniforms remain unsupported and produce an error — use explicit UBO blocks instead.
- Internal deko3d constbuf layout and numbering schemes are used, as opposed to nouveau's.
- `gl_FragCoord` always uses the Y axis convention specified in the flags during the creation of a deko3d device. `layout (origin_upper_left)` has no effect whatsoever and produces a warning, while `layout (pixel_center_integer)` is not supported at all and produces an error.
- Integer divisions and modulo operations with non-constant divisors decay to floating point division, and generate a warning. Well written shaders should avoid these operations for performance and accuracy reasons. (Also note that unmodified nouveau, in order to comply with the GL standard, emulates integer division/module with a software routine that has been removed in UAM)
- 64-bit floating point divisions and square roots can only be approximated with native hardware instructions. This results in loss of accuracy, and as such these operations should be avoided, and they generate a warning as well. (Also note that likewise, unmodified nouveau uses a software routine that has been removed in UAM)
- Transform feedback is not supported.
- GLSL shader subroutines (`ARB_shader_subroutine`) are not supported.
- There is no concept of shader linking. Separable programs (`ARB_separate_shader_objects`) are always in effect.
- The compiler is based on mesa 19.0.8 sources; however several cherrypicked bugfixes from mesa 19.1 and up have been applied.
- Numerous codegen differences:
	- Added **Maxwell dual issue** scheduling support based on the groundwork laid out by karolherbst's [dual_issue_v3](https://github.com/karolherbst/mesa/commits/dual_issue_v3) branch, and enhanced with new experimental findings.
	- Removed bound checks in SSBO accesses.
	- Removed bound checks in atomic accesses.
	- Removed bound checks in image accesses.
	- Multisampled texture lookups use optimized bitwise logic with hardcoded sample positions instead of requiring helper data in the driver constbuf.
	- Multisampled image operations use `TXQ` instead of requiring helper data in the driver constbuf.
	- Non-bindless image operations are supported natively instead of being emulated with bindless operations.
	- SSBO size calculations use unsigned math instead of signed math, which results in better codegen.
	- `ballotARB()` called with a constant argument now results in optimal codegen using the PT predicate register.
	- **Bugfixes**:
		- Bindless texture queries were broken.
		- `IMAD` instruction encoding with negated operands was broken.
	- Minor changes done to match properties observed in official shader code:
		- `MOV Rd,RZ` is now preferred to `MOV32I Rd,0`.
		- `LDG`/`STG` instructions are used for SSBO accesses instead of `LD`/`ST`.
		- Shader programs are properly padded out to a size that is a multiple of 64 bytes.

## Tests

```bash
meson setup builddir_test
meson test -C builddir_test
```

The SPIR-V parser has 38 unit tests covering header validation, type parsing, decorations, constants, variables, functions, and a full integration test with a realistic vertex shader module.

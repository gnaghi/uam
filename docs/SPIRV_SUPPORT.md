# SPIR-V Support in uam

## Motivation

The SDL_GPU deko3d backend provides shaders as SPIR-V binaries. uam only supported GLSL input. This work adds a complete SPIR-V input path to produce DKSH output, enabling SDL_GPU shader compilation on Nintendo Switch.

## Architecture

```
Existing path:   GLSL source  --> glsl_frontend --> TGSI --> nv50_ir_from_tgsi --> NV50_IR --> Maxwell --> DKSH
New path:        SPIR-V binary --> spirv_frontend ------------> nv50_ir_from_spirv --> NV50_IR --> Maxwell --> DKSH
```

The SPIR-V path bypasses TGSI entirely and translates directly to NV50_IR. The entire backend (optimization, register allocation, Maxwell emission, DKSH packaging) is shared with the GLSL path.

## Files Added

| File | Lines | Description |
|------|------:|-------------|
| `source/spirv/spirv.h` | 5560 | Khronos SPIR-V header (opcodes, enums) |
| `source/spirv/GLSL.std.450.h` | 115 | GLSL.std.450 extended instruction set |
| `source/spirv_frontend.h` | 256 | SPIR-V parser types and API |
| `source/spirv_frontend.cpp` | 946 | Two-pass SPIR-V binary parser |
| `mesa-imported/codegen/nv50_ir_from_spirv.cpp` | 2824 | SPIR-V to NV50_IR converter |
| `tests/test_spirv_parser.cpp` | 1303 | 38 unit tests for the parser |

## Files Modified

| File | Change |
|------|--------|
| `mesa-imported/pipe/p_defines.h` | Added `PIPE_SHADER_IR_SPIRV = 3` |
| `mesa-imported/codegen/nv50_ir.h` | Added `makeFromSPIRV()` declaration |
| `mesa-imported/codegen/nv50_ir.cpp` | Added `PIPE_SHADER_IR_SPIRV` case |
| `source/compiler_iface.h` | Added `CompileSpirv()` method |
| `source/compiler_iface.cpp` | Implemented `CompileSpirv()` + SPIR-V slot assignment |
| `source/uam.h` | Added `uam_compile_spirv()` C API |
| `source/uam.cpp` | Implemented `uam_compile_spirv()` wrapper |
| `source/main.cpp` | Added `-i spirv` option + auto-detection |
| `source/meson.build` | Added `spirv_frontend.cpp` |
| `mesa-imported/codegen/meson.build` | Added `nv50_ir_from_spirv.cpp` |
| `meson.build` | Added source include path + test target |

Total: **~11,200 lines added** across 17 files.

## SPIR-V Frontend Parser

Self-contained parser with no dependency on spirv-tools or spirv-cross. Two-pass design:

1. **Pass 1** — Types, decorations, constants, variables, names, entry points
2. **Pass 2** — Function bodies, basic blocks, instruction streams

Parsed data is stored in a `spirv::Program` structure with ID-indexed lookups (pre-allocated to the SPIR-V `bound` size).

### Supported OpCodes (parser)

- **Types:** OpTypeVoid, OpTypeBool, OpTypeInt, OpTypeFloat, OpTypeVector, OpTypeMatrix, OpTypeArray, OpTypeRuntimeArray, OpTypeStruct, OpTypePointer, OpTypeFunction, OpTypeImage, OpTypeSampler, OpTypeSampledImage
- **Decorations:** Location, Binding, DescriptorSet, BuiltIn, Block, BufferBlock, Flat, NoPerspective, Centroid, Offset, MatrixStride, ArrayStride, ColMajor, RowMajor
- **Constants:** OpConstant, OpConstantTrue, OpConstantFalse, OpConstantComposite, OpConstantNull, OpSpecConstant
- **Variables:** OpVariable (all storage classes)
- **Debug:** OpName, OpMemberName
- **Extensions:** OpExtInstImport
- **Entry:** OpEntryPoint (matches requested pipeline stage)

## NV50_IR Converter

Direct translation of SPIR-V instructions to NV50_IR, following the pattern of the existing `nv50_ir_from_tgsi.cpp`.

### Supported Operations

| Category | SPIR-V Operations | NV50_IR Mapping |
|----------|------------------|-----------------|
| **Arithmetic** | OpFAdd, OpFSub, OpFMul, OpFDiv, OpFNegate, OpIAdd, OpISub, OpIMul, OpSDiv, OpUDiv, OpSRem, OpSMod, OpUMod, OpFRem, OpFMod | OP_ADD, OP_SUB, OP_MUL, OP_RCP+OP_MUL, OP_NEG |
| **Bitwise** | OpBitwiseAnd, OpBitwiseOr, OpBitwiseXor, OpNot, OpShiftLeftLogical, OpShiftRightLogical, OpShiftRightArithmetic | OP_AND, OP_OR, OP_XOR, OP_NOT, OP_SHL, OP_SHR |
| **Comparison** | OpFOrdEqual, OpFOrdLessThan, OpFOrdGreaterThan, etc. (all ordered/unordered float + signed/unsigned int) | OP_SET with condition codes |
| **Conversion** | OpConvertFToS, OpConvertFToU, OpConvertSToF, OpConvertUToF, OpBitcast | OP_CVT |
| **Composite** | OpCompositeExtract, OpCompositeConstruct, OpVectorShuffle | Direct value routing |
| **Memory** | OpLoad, OpStore, OpAccessChain | OP_LOAD, OP_STORE + offset calculation |
| **Control flow** | OpBranch, OpBranchConditional, OpPhi, OpReturn, OpKill | BRA, PHI, RET, DISCARD |
| **Logic** | OpLogicalAnd, OpLogicalOr, OpLogicalNot, OpLogicalEqual, OpLogicalNotEqual, OpSelect | OP_AND, OP_OR, OP_NOT, OP_SET, OP_SLCT |

### GLSL.std.450 Extended Instructions

sin, cos, tan, asin, acos, atan, atan2, sinh, cosh, tanh, exp, exp2, log, log2, pow, sqrt, inversesqrt, floor, ceil, round, trunc, fract, fabs, sign, fmin, fmax, fclamp, mix (fma-based), step, smoothstep, length, distance, normalize, cross, dot, reflect, refract, faceforward, determinant, matrixinverse (stubbed)

### Built-in Variables

- **Vertex:** gl_Position (output), gl_VertexID, gl_InstanceID
- **Fragment:** gl_FragCoord, gl_FrontFacing, gl_FragDepth

### Fragment Shader Interpolation

- Perspective-correct interpolation via PINTERP
- Linear (noPerspective) interpolation via LINTERP
- Flat interpolation (integer attributes)
- Centroid qualifier support

### Uniform Buffer Objects

UBOs are accessed through FILE_MEMORY_CONST with deko3d constbuf numbering. Binding decorations map to constbuf indices (offset by 3 to skip driver-internal constbufs).

### Varying Slot Assignment

SPIR-V Location decorations map directly to NvAttrib_Generic slots for vertex inputs and inter-stage varyings. Built-in variables are mapped to their TGSI semantic equivalents so the existing `nvc0_program_assign_varying_slots` infrastructure works unchanged.

## Known Limitations

- **Texture sampling** is stubbed (warns and produces MOV 0). Full texture support requires OP_TEX/OP_TXL/OP_TXF integration.
- **Geometry, tessellation, and compute** stages are untested. The converter handles vertex and fragment stages.
- **SSBO and image access** from SPIR-V are not yet implemented.
- **Phi nodes** use a simplified first-available strategy rather than full phi resolution.
- **Spec constants** are parsed but treated as regular constants.

## Testing

38 unit tests validate the SPIR-V parser:

```
Header:              3 tests (magic validation, minimum size, field extraction)
Scalar types:        4 tests (void, bool, int, float)
Composite types:     6 tests (vector, matrix, array, struct, pointer, function)
Decorations:         6 tests (location, binding, builtin, interpolation, block, member)
Constants:           4 tests (scalar, true/false, composite, null)
Variables:           3 tests (input, output, uniform)
Names & imports:     2 tests (OpName, ExtInstImport)
Entry point:         2 tests (stage matching, wrong stage filtering)
Functions:           3 tests (structure, multiple blocks, parameters)
Size computation:    1 test  (scalar, vector, array, struct byte sizes)
Integration:         1 test  (realistic vertex passthrough module, ~30 assertions)
Edge cases:          3 tests (null returns, decoration accumulation, stage propagation)
```

Run with: `meson test -C builddir_test spirv_parser`

## API Usage

### C API

```c
uam_compiler *c = uam_create_compiler(DkStage_Vertex);

// Read .spv file into memory...
if (uam_compile_spirv(c, spirv_data, spirv_size)) {
    size_t size = uam_get_code_size(c);
    void *dksh = malloc(size);
    uam_write_code(c, dksh);
}

uam_free_compiler(c);
```

### CLI

```bash
# Explicit format
uam -s vert -i spirv -o output.dksh shader.spv

# Auto-detected from SPIR-V magic number
uam -s vert -o output.dksh shader.spv

# GLSL (unchanged)
uam -s frag -o output.dksh shader.glsl
```

## Design Decisions

1. **No TGSI intermediary** — Direct SPIR-V to NV50_IR avoids TGSI limitations and is more efficient.
2. **Khronos spirv.h** — Official headers, no custom opcode definitions.
3. **Self-contained parser** — No dependency on spirv-tools or spirv-cross, keeps uam minimal.
4. **BuiltIn to TGSI semantic mapping** — Reuses existing slot assignment infrastructure unchanged.
5. **No GLSL frontend dependency** — `CompileSpirv()` bypasses `glsl_frontend_init()` entirely.

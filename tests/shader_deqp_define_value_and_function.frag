#version 460
// Transpiler output for: dEQP-GLES2.functional.shaders.preprocessor.definitions.define_value_and_function_fragment
// Original ES 1.00 code with ${DECLARATIONS} and ${OUTPUT} expanded:
//   precision mediump float;
//   bool isOk(float a, float b, float eps) { return (abs(a-b) <= (eps*abs(b) + eps)); }
//   uniform float ref_out0;   // expected = 6.0
//   float out0;
//   #define VALUE (1.5 + 2.5)
//   #define FUNCTION(__LINE__, b) __LINE__+b
//   void main() {
//     out0 = FUNCTION(VALUE, ((0.2) + 1.8));
//     bool RES = isOk(out0, ref_out0, 0.05);
//     gl_FragColor = vec4(RES, RES, RES, 1.0);
//   }
//
// Transpiler transforms:
//   - precision → removed
//   - uniform float ref_out0 → UBO at binding 0
//   - gl_FragColor → layout(location=0) out
//   - isOk() function unchanged
//   - preprocessor handled at compile time

layout(location = 0) out vec4 _gl_FragColor;

layout(std140, binding = 0) uniform UBO_FS_0 {
    float ref_out0;
};

bool isOk(float a, float b, float eps) {
    return (abs(a - b) <= (eps * abs(b) + eps));
}

void main() {
    float out0;
    out0 = (1.5 + 2.5) + ((0.2) + 1.8);
    bool RES = isOk(out0, ref_out0, 0.05);
    _gl_FragColor = vec4(float(RES), float(RES), float(RES), 1.0);
}

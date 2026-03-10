#version 460
// dEQP-GLES2.functional.shaders.functions.datatypes.float_float (fragment variant)
// ES 1.00 original: float func(float a) { return -a; }
// Transpiled to GLSL 4.60

layout(location = 0) in float v_in0;
layout(location = 0) out vec4 _gl_FragColor;

layout(std140, binding = 0) uniform UBO_FS_0 {
    float ref_out0;
};

bool isOk(float a, float b, float eps) {
    return (abs(a - b) <= (eps * abs(b) + eps));
}

float func(float a) {
    return -a;
}

void main() {
    float out0;
    out0 = func(v_in0);
    bool RES = isOk(out0, ref_out0, 0.05);
    _gl_FragColor = vec4(float(RES), float(RES), float(RES), 1.0);
}

#version 460
// dEQP pattern: inout parameter (common in function tests)
layout(location = 0) in float v_in0;
layout(location = 0) out vec4 _gl_FragColor;

layout(std140, binding = 0) uniform UBO_FS_0 {
    float ref_out0;
};

bool isOk(float a, float b, float eps) {
    return (abs(a - b) <= (eps * abs(b) + eps));
}

void negate(inout float x) {
    x = -x;
}

void main() {
    float out0 = v_in0;
    negate(out0);
    bool RES = isOk(out0, ref_out0, 0.05);
    _gl_FragColor = vec4(float(RES), float(RES), float(RES), 1.0);
}

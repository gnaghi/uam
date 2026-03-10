#version 460
// dEQP pattern with multiple outputs (like vec2 output)
layout(location = 0) in vec2 v_in0;
layout(location = 0) out vec4 _gl_FragColor;

layout(std140, binding = 0) uniform UBO_FS_0 {
    vec2 ref_out0;
};

bool isOk(float a, float b, float eps) {
    return (abs(a - b) <= (eps * abs(b) + eps));
}

vec2 func(vec2 a) {
    return vec2(-a.y, a.x);
}

void main() {
    vec2 out0 = func(v_in0);
    bool RES = isOk(out0.x, ref_out0.x, 0.05);
    RES = RES && isOk(out0.y, ref_out0.y, 0.05);
    _gl_FragColor = vec4(float(RES), float(RES), float(RES), 1.0);
}

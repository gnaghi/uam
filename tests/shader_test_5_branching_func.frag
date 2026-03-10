#version 460

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform FragUniforms {
    vec4 u_ref;
};

float clampToRange(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

void main() {
    float r = clampToRange(u_ref.x, 0.0, 1.0);
    float g = clampToRange(u_ref.y, 0.0, 1.0);
    fragColor = vec4(r, g, 0.0, 1.0);
}

#version 460

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform FragUniforms {
    vec4 u_ref;
};

float square(float x) {
    return x * x;
}

float computeLength(float a, float b) {
    return sqrt(square(a) + square(b));
}

void main() {
    float len = computeLength(u_ref.x, u_ref.y);
    fragColor = vec4(len, 0.0, 0.0, 1.0);
}

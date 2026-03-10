#version 460
// Tests void function calls (OpReturn without value)

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

vec4 result;

void computeResult(float x, float y) {
    result = vec4(x, y, x * y, 1.0);
}

void main() {
    computeResult(0.5, 0.7);
    fragColor = result;
}

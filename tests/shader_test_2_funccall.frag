#version 460

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

float addValues(float a, float b) {
    return a + b;
}

void main() {
    float result = addValues(1.5 + 2.5, (0.2) + 1.8);
    fragColor = vec4(result / 6.0, 0.0, 0.0, 1.0);
}

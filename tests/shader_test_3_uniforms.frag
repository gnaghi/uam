#version 460

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform FragUniforms {
    vec4 ref_out0;
};

void main() {
    fragColor = ref_out0;
}

#version 460
// Mimics what transpiler produces for dEQP shader tests
// dEQP uses "in0" input and "ref_out0" uniform for comparison

layout(location = 0) in vec4 a_position;
layout(location = 1) in float in0;
layout(location = 0) out float v_out0;

layout(std140, binding = 0) uniform Transforms {
    mat4 u_mvp;
};

void main() {
    gl_Position = u_mvp * a_position;
    v_out0 = in0;
}

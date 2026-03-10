#version 460

layout(location = 0) in vec4 a_position;
layout(location = 0) out vec4 v_color;

void main() {
    gl_Position = a_position;
    v_color = vec4(1.0, 0.5, 0.0, 1.0);
}

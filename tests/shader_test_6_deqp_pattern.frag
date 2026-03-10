#version 460
// Mimics what transpiler produces for dEQP shader tests
// Fragment shader with ref_out0 uniform for comparison

layout(location = 0) in float v_out0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform FragUniforms {
    float ref_out0;
};

void main() {
    float out0 = v_out0;
    // dEQP comparison: if out0 matches ref_out0, output white; else red
    float diff = abs(out0 - ref_out0);
    if (diff < 0.05)
        fragColor = vec4(1.0, 1.0, 1.0, 1.0);
    else
        fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}

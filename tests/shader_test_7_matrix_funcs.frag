#version 460
// Tests matrix operations with function calls (like dEQP matrix tests)

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform FragUniforms {
    mat2 ref_mat;
    float ref_out0;
};

float computeDeterminant(mat2 m) {
    return m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

mat2 invertMatrix(mat2 m) {
    float det = computeDeterminant(m);
    if (abs(det) < 0.001) return mat2(1.0);
    float invDet = 1.0 / det;
    return mat2(
        m[1][1] * invDet, -m[0][1] * invDet,
        -m[1][0] * invDet, m[0][0] * invDet
    );
}

void main() {
    mat2 inv = invertMatrix(ref_mat);
    float det = computeDeterminant(inv);
    fragColor = vec4(det, 0.0, 0.0, 1.0);
}

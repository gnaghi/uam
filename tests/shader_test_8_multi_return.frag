#version 460
// Tests function with multiple return paths (generates multiple OpReturnValue)

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

int fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

void main() {
    int fib5 = fibonacci(5);
    fragColor = vec4(float(fib5) / 5.0, 0.0, 0.0, 1.0);
}

#version 460
layout(location = 0) out vec4 _gl_FragColor;

layout(std140, binding = 0) uniform UBO_FS_0 {
    float ref_out0;
};

bool isOk (float a, float b, float eps) { return (abs(a-b) <= (eps*abs(b) + eps)); }

#    define        VALUE            (1.5 + 2.5)
#    define        FUNCTION(__LINE__, b)    __LINE__+b

void main()
{
    float out0;
    out0 = FUNCTION(VALUE, ((0.2) + 1.8) );
    bool RES = isOk(out0, ref_out0, 0.05);
    _gl_FragColor = vec4(float(RES), float(RES), float(RES), 1.0);
}

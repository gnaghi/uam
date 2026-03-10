precision mediump float;
bool isOk (float a, float b, float eps) { return (abs(a-b) <= (eps*abs(b) + eps)); } uniform float ref_out0; float out0;
#    define        VALUE            (1.5 + 2.5)
#    define        FUNCTION(__LINE__, b)    __LINE__+b

void main()
{
    out0 = FUNCTION(VALUE, ((0.2) + 1.8) );
    bool RES = isOk(out0, ref_out0, 0.05);
    gl_FragColor = vec4(RES, RES, RES, 1.0);
}

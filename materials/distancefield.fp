varying mediump vec2 var_texcoord0;

uniform lowp sampler2D texture_sampler;
uniform lowp vec4 tint;

const float threshold = .01;

void main()
{
    // Pre-multiply alpha since all runtime textures already are
    vec4 c = texture2D(texture_sampler, var_texcoord0.xy);
    float res = smoothstep(.5-threshold, .5+threshold, c.x);
    lowp vec4 tint_pm = vec4(tint.xyz * res, res);

    //Black and white
    // res = vec4(res,c.w) * tint_pm;
    // float d = (res.r + res.g + res.b)/3.0;
    // res.x = d;
    // res.y = d;
    // res.z = d;
    // tint_pm = vec4(1, 1, 1, tint.w);

    gl_FragColor = tint_pm;
}

uniform sampler2D sampler;

varying vec2 texcoord0;

uniform sampler3D clut;

void main()
{
    vec4 tex = texture2D(sampler, texcoord0);

    tex.rgb = texture3D(clut, tex.rgb).rgb * tex.a;

    gl_FragColor = tex;
}

#version 140
uniform sampler2D sampler;

in vec2 texcoord0;

uniform sampler3D clut;

out vec4 fragColor;

void main()
{
    vec4 tex = texture(sampler, texcoord0);

    tex.rgb = texture(clut, tex.rgb).rgb * tex.a;

    fragColor = tex;
}

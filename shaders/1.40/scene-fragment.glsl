#version 140

uniform sampler2D sampler;
uniform vec4 modulation;
uniform float saturation;

in vec2 varyingTexCoords;

out vec4 fragColor;

void main()
{
    vec4 tex = texture(sampler, varyingTexCoords);

    if (saturation != 1.0) {
        tex.rgb = mix(vec3(dot(vec3(0.30, 0.59, 0.11), tex.rgb)), tex.rgb, saturation);
    }

    tex *= modulation;

#ifdef KWIN_SHADER_DEBUG
    tex.g = min(tex.g + 0.5, 1.0);
#endif

    fragColor = tex;
}

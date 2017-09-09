#version 460 core

layout (set = 0, binding = 0) uniform sampler2D tex;

layout (set = 0, binding = 1) uniform UBO {
    mat4 modelViewProjectionMatrix;
    vec4 modulation;
    vec4 color;
    float saturation;
    float crossFadeProgress;
};

layout (location = 0) in vec2 texcoord;
layout (location = 0) out vec4 fragColor;

void main(void)
{
    vec4 texel = texture(tex, texcoord) * modulation;
    texel.rgb = mix(vec3(dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722))), texel.rgb, saturation);

    fragColor = texel;
}

#version 460 core

layout (set = 0, binding = 0) uniform texture2D tex[2];
layout (set = 0, binding = 1) uniform sampler samp;

layout (set = 0, binding = 2) uniform UBO {
    mat4 modelViewProjectionMatrix;
    vec4 modulation;
    float saturation;
    float crossFadeProgress;
};

layout (location = 0) in vec2 texcoord0;
layout (location = 1) in vec2 texcoord1;

layout (location = 0) out vec4 fragColor;

void main(void)
{
    vec4 texel0 = texture(sampler2D(tex[0], samp), texcoord0);
    vec4 texel1 = texture(sampler2D(tex[1], samp), texcoord1);

    fragColor = mix(texel0, texel1, crossFadeProgress);
}

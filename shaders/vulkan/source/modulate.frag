#version 460 core

layout (set = 0, binding = 0) uniform sampler2D tex;

layout (set = 0, binding = 1) uniform UBO {
    mat4 modelViewProjectionMatrix;
    vec4 modulation;
    float saturation;
    float crossFadeProgress;
};

layout (location = 0) in vec2 texcoord;
layout (location = 0) out vec4 fragColor;

void main(void)
{
    fragColor = texture(tex, texcoord) * modulation;
}

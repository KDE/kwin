#version 460 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec2 texcoord;

out gl_PerVertex {
    vec4 gl_Position;
};

layout (location = 0) out vec2 out_texcoord;

layout (set = 0, binding = 1) uniform UBO {
    mat4 modelViewProjectionMatrix;
    vec4 modulation;
    vec4 color;
    float saturation;
    float crossFadeProgress;
};

void main(void)
{
    out_texcoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}

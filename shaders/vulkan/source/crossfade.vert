#version 460 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec2 texcoord0;
layout (location = 2) in vec2 texcoord1;

out gl_PerVertex {
    vec4 gl_Position;
};

layout (location = 0) out vec2 out_texcoord0;
layout (location = 1) out vec2 out_texcoord1;

layout (set = 0, binding = 2) uniform UBO {
    mat4 modelViewProjectionMatrix;
    vec4 modulation;
    float saturation;
    float crossFadeProgress;
};

void main(void)
{
    out_texcoord0 = texcoord0;
    out_texcoord1 = texcoord1;
    gl_Position = modelViewProjectionMatrix * position;
}

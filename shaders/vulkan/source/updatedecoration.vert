#version 460 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec2 in_texcoord;

out gl_PerVertex {
    vec4 gl_Position;
};

layout (location = 0) out vec2 out_texcoord;

void main(void)
{
    out_texcoord = in_texcoord;
    gl_Position = position;
}

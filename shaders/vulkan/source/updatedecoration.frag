#version 460 core

layout (set = 0, binding = 0) uniform texture2D tex[4];
layout (set = 0, binding = 1) uniform sampler samp; 

layout (location = 0) in vec2 texcoord;
layout (location = 0) out vec4 fragColor;

layout (push_constant) uniform TextureIndex {
    uint index;
};

void main(void)
{
    fragColor = texture(sampler2D(tex[index], samp), texcoord);
}

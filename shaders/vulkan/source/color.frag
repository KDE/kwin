#version 460 core

layout (location = 0) out vec4 fragColor;

layout (set = 0, binding = 0) uniform UBO {
    mat4 modelViewProjectionMatrix;
    vec4 color;
};

void main(void)
{
    fragColor = color;
}

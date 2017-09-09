#version 460 core

layout (location = 0) in vec4 position;

out gl_PerVertex {
    vec4 gl_Position;
};

layout (set = 0, binding = 0) uniform UBO {
    mat4 modelViewProjectionMatrix;
    vec4 color;
};

void main(void)
{
    gl_Position = modelViewProjectionMatrix * position;
}

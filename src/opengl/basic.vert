#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 uvcoords;

layout(location = 0) out vec2 texcoords;

layout(set = 0, binding = 0) uniform vertexUniforms {
    mat4 matrix;
} modelViewProjectionMatrix;

void main()
{
    texcoords = uvcoords.xy;
    gl_Position = modelViewProjectionMatrix.matrix * position;
}

#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "colormanagement.glsl"

layout(location = 0) in vec2 texcoords;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

void main()
{
    out_color = texture(textureSampler, texcoords);
}

void colorManagement()
{
    out_color = texture(textureSampler, texcoords);
    out_color = sourceEncodingToNitsInDestinationColorspace(out_color);
    out_color = nitsToDestinationEncoding(out_color);
}

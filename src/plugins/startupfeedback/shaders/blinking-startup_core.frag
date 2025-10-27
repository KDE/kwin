#version 140

#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform vec4 geometryColor;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    vec4 tex = texture(sampler, texcoord0);
    if (tex.a != 1.0) {
        tex = geometryColor;
    }
    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    fragColor = nitsToDestinationEncoding(tex);
}

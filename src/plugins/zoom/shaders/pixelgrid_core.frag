#version 140

#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform int textureWidth;
uniform int textureHeight;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    vec2 texSize = vec2(textureWidth, textureHeight);
    vec2 samplePosition = texcoord0 * texSize;
    vec2 pixelCenter = floor(samplePosition) + vec2(0.5);
    vec2 pixelCenterOffset = samplePosition - pixelCenter;

    vec4 tex;
    if (abs(pixelCenterOffset.x) > 0.4 || abs(pixelCenterOffset.y) > 0.4) {
        tex = vec4(0, 0, 0, 1);
    } else {
        tex = texture(sampler, pixelCenter / texSize);
    }

    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    fragColor = nitsToDestinationEncoding(tex);
}

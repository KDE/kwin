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
    vec2 pixelCenterDistance = abs(samplePosition - pixelCenter);

    float t = smoothstep(0.4, 0.5, max(pixelCenterDistance.x, pixelCenterDistance.y));
    vec4 tex = mix(texture(sampler, pixelCenter / texSize), vec4(0, 0, 0, 1), t);
    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    fragColor = nitsToDestinationEncoding(tex);
}

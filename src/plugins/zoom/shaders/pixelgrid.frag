#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform int textureWidth;
uniform int textureHeight;

varying vec2 texcoord0;

void main()
{
    vec2 texSize = vec2(textureWidth, textureHeight);
    vec2 samplePosition = texcoord0 * texSize;
    vec2 pixelCenter = floor(samplePosition) + vec2(0.5);
    vec2 pixelCenterDistance = abs(samplePosition - pixelCenter);

    vec4 tex;
    if (pixelCenterDistance.x > 0.4 || pixelCenterDistance.y > 0.4) {
        tex = vec4(0, 0, 0, 1);
    } else {
        tex = texture2D(sampler, pixelCenter / texSize);
    }

    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    gl_FragColor = nitsToDestinationEncoding(tex);
}

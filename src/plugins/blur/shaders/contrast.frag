#include "colormanagement.glsl"

uniform sampler2D texUnit;
uniform mat4 colorMatrix;
uniform float offset;
uniform vec2 halfpixel;

varying vec2 uv;

void main(void)
{
    vec4 sum = texture2D(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture2D(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture2D(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture2D(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture2D(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture2D(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture2D(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture2D(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;

    vec4 linear = sourceEncodingToNitsInDestinationColorspace(sum / 12);
    gl_FragColor = nitsToDestinationEncoding(linear * colorMatrix);
}

#version 140

#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform int textureWidth;
uniform int textureHeight;

in vec2 texcoord0;

out vec4 fragColor;

const mat4 yuvToRgb = mat4(1, 0, 1.4746001, 0,
                           1, -0.16455314, -0.57135314, 0,
                           1, 1.8814, 0, 0,
                           -1, -0.8584234, 0.28567657, 1);

void main()
{
    mat4 rgbToYuv = inverse(yuvToRgb);
    vec4 tex = texture(sampler, texcoord0);
    tex = tex * rgbToYuv;
    fragColor = tex * yuvToRgb;
}
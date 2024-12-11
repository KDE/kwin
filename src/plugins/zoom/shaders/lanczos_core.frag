#version 140

#define PI 3.1415926535897932384626433

#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform int textureWidth;
uniform int textureHeight;

in vec2 texcoord0;

out vec4 fragColor;

float sinc(float x)
{
    if (x == 0.0) {
        return 1.0;
    } else {
        return sin(PI * x) / (x * PI);
    }
}

float lanczos(vec2 position, float a)
{
    float d = length(position);
    if (d >= a) {
        return 0.0;
    } else {
        return sinc(d) * sinc(d / a);
    }
}

void main()
{
    const int a = 2;

    float verticalStep = 1.0 / textureHeight;
    float horizontalStep = 1.0 / textureWidth;

    vec4 accTex = vec4(0.0);
    float accWeight = 0.0;
    for (int i = -a + 1; i < a; ++i) {
        for (int j = -a + 1; j < a; ++j) {
            vec4 tex = texture(sampler, texcoord0 + vec2(j * horizontalStep, i * verticalStep));
            float weight = lanczos(vec2(j, i), a);

            accTex += tex * weight;
            accWeight += weight;
        }
    }

    vec4 tex = accTex / accWeight;
    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    fragColor = nitsToDestinationEncoding(tex);
}

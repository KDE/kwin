#version 140

uniform sampler2D texUnit;
uniform vec2 offsets[16];
uniform vec4 kernel[16];

in vec2 varyingTexCoords;
out vec4 fragColor;

void main(void)
{
    vec4 sum = texture(texUnit, varyingTexCoords.st) * kernel[0];
    for (int i = 1; i < 16; i++) {
        sum += texture(texUnit, varyingTexCoords.st - offsets[i]) * kernel[i];
        sum += texture(texUnit, varyingTexCoords.st + offsets[i]) * kernel[i];
    }
    fragColor = sum;
}


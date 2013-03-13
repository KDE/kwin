uniform sampler2D texUnit;
uniform vec2 offsets[16];
uniform vec4 kernel[16];

varying vec2 varyingTexCoords;

void main(void)
{
    vec4 sum = texture2D(texUnit, varyingTexCoords.st) * kernel[0];
    for (int i = 1; i < 16; i++) {
        sum += texture2D(texUnit, varyingTexCoords.st - offsets[i]) * kernel[i];
        sum += texture2D(texUnit, varyingTexCoords.st + offsets[i]) * kernel[i];
    }
    gl_FragColor = sum;
}


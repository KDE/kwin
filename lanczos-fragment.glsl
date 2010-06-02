uniform sampler2D texUnit;
uniform vec2 offsets[25];
uniform vec4 kernel[25];
uniform int kernelSize;

void main(void)
{
    vec4 sum = texture2D(texUnit, gl_TexCoord[0].st) * kernel[0];
    for (int i = 1; i < kernelSize; i++) {
        sum += texture2D(texUnit, gl_TexCoord[0].st - offsets[i]) * kernel[i];
        sum += texture2D(texUnit, gl_TexCoord[0].st + offsets[i]) * kernel[i];
    }
    gl_FragColor = sum;
}


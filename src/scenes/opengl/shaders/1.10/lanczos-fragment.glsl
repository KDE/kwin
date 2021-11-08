uniform sampler2D sampler;
uniform vec2 offsets[16];
uniform vec4 kernel[16];

varying vec2 texcoord0;

void main(void)
{
    vec4 sum = texture2D(sampler, texcoord0.st) * kernel[0];
    for (int i = 1; i < 16; i++) {
        sum += texture2D(sampler, texcoord0.st - offsets[i]) * kernel[i];
        sum += texture2D(sampler, texcoord0.st + offsets[i]) * kernel[i];
    }
    gl_FragColor = sum;
}


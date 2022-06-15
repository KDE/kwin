#version 140

uniform sampler2D texUnit;
uniform float offset;
uniform vec2 renderTextureSize;
uniform vec2 halfpixel;

out vec4 fragColor;

void main(void)
{
    vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);

    vec4 sum = texture(texUnit, uv) * 4.0;
    sum += texture(texUnit, uv - halfpixel.xy * offset);
    sum += texture(texUnit, uv + halfpixel.xy * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset);
    sum += texture(texUnit, uv - vec2(halfpixel.x, -halfpixel.y) * offset);

    fragColor = sum / 8.0;
}

uniform sampler2D texUnit;
uniform float offset;
uniform vec2 renderTextureSize;
uniform vec2 halfpixel;

void main(void)
{
    vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);

    vec4 sum = texture2D(texUnit, uv) * 4.0;
    sum += texture2D(texUnit, uv - halfpixel.xy * offset);
    sum += texture2D(texUnit, uv + halfpixel.xy * offset);
    sum += texture2D(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset);
    sum += texture2D(texUnit, uv - vec2(halfpixel.x, -halfpixel.y) * offset);

    gl_FragColor = sum / 8.0;
}

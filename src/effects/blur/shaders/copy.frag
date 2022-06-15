uniform sampler2D texUnit;
uniform vec2 renderTextureSize;
uniform vec4 blurRect;

void main(void)
{
    vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);
    gl_FragColor = texture2D(texUnit, clamp(uv, blurRect.xy, blurRect.zw));
}

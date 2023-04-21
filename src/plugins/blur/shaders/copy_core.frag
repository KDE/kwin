#version 140

uniform sampler2D texUnit;
uniform vec2 renderTextureSize;
uniform vec4 blurRect;

out vec4 fragColor;

void main(void)
{
    vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);
    fragColor = texture(texUnit, clamp(uv, blurRect.xy, blurRect.zw));
}

#version 140

uniform sampler2D texUnit;
uniform float offset;
uniform vec2 renderTextureSize;
uniform vec2 halfpixel;

uniform vec2 noiseTextureSize;
uniform vec2 texStartPos;

out vec4 fragColor;

void main(void)
{
    vec2 uvNoise = vec2((texStartPos.xy + gl_FragCoord.xy) / noiseTextureSize);

    fragColor = vec4(texture(texUnit, uvNoise).rrr, 0);
}

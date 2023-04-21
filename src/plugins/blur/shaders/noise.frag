uniform sampler2D texUnit;
uniform float offset;
uniform vec2 renderTextureSize;
uniform vec2 halfpixel;

uniform vec2 noiseTextureSize;
uniform vec2 texStartPos;

void main(void)
{
    vec2 uvNoise = vec2((texStartPos.xy + gl_FragCoord.xy) / noiseTextureSize);

    gl_FragColor = vec4(texture2D(texUnit, uvNoise).rrr, 0);
}

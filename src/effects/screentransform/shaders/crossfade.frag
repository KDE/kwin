uniform sampler2D previousTexture;
uniform sampler2D currentTexture;
uniform float blendFactor;

varying vec2 texcoord0;

void main()
{
    vec4 previous = texture2D(previousTexture, texcoord0);
    vec4 current = texture2D(currentTexture, texcoord0);
    gl_FragColor = mix(previous, current, blendFactor);
}

#version 140

uniform sampler2D previousTexture;
uniform sampler2D currentTexture;
uniform float blendFactor;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    vec4 previous = texture(previousTexture, texcoord0);
    vec4 current = texture(currentTexture, texcoord0);
    fragColor = mix(previous, current, blendFactor);
}

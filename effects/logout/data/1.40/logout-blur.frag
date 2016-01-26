#version 140

uniform sampler2D sampler;
uniform float u_alphaProgress;

in vec2 texcoord0;
out vec4 fragColor;

void main() {
    vec4 texel = texture(sampler, texcoord0, 1.75);
    texel.a = u_alphaProgress;

    fragColor = texel;
}

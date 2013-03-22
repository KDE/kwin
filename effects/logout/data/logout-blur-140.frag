#version 140

uniform sampler2D sampler;
uniform float u_alphaProgress;

in vec2 varyingTexCoords;
out vec4 fragColor;

void main() {
    vec4 texel = texture(sampler, varyingTexCoords, 1.75);
    texel.a = u_alphaProgress;

    fragColor = texel;
}

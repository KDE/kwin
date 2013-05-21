#version 140
uniform float u_alpha;

in vec2 varyingTexCoords;

out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0, 0.0, 0.0, u_alpha*varyingTexCoords.s);
}

#version 140
uniform vec4 u_frontColor;
uniform vec4 u_backColor;

in vec2 varyingTexCoords;

out vec4 fragColor;

void main()
{
    fragColor = u_frontColor*(1.0-varyingTexCoords.s) + u_backColor*varyingTexCoords.s;
}

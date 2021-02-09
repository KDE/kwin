#version 140
uniform vec4 u_frontColor;
uniform vec4 u_backColor;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    fragColor = u_frontColor*(1.0-texcoord0.s) + u_backColor*texcoord0.s;
}

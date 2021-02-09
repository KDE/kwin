#version 140
uniform float u_alpha;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0, 0.0, 0.0, u_alpha*texcoord0.s);
}

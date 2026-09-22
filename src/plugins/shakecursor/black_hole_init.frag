#version 140

in vec2 texcoord0;
out vec4 fragColor;

uniform vec2 size;

void main()
{
    fragColor = vec4(texcoord0 * size, 1.0, 0.0);
}

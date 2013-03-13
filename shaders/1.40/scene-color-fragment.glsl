#version 140

uniform vec4 geometryColor;

out vec4 fragColor;

void main()
{
    fragColor = geometryColor;
}

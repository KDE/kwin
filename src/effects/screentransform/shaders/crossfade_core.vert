#version 140

uniform mat4 modelViewProjectionMatrix;

in vec2 position;
in vec2 texcoord;

out vec2 texcoord0;

void main()
{
    gl_Position = modelViewProjectionMatrix * vec4(position, 0.0, 1.0);
    texcoord0 = texcoord;
}

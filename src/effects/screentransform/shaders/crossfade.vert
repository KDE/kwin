uniform mat4 modelViewProjectionMatrix;

attribute vec2 position;
attribute vec2 texcoord;

varying vec2 texcoord0;

void main()
{
    gl_Position = modelViewProjectionMatrix * vec4(position, 0.0, 1.0);
    texcoord0 = texcoord;
}

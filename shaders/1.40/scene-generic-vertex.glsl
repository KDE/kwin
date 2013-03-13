#version 140

uniform mat4 projection;
uniform mat4 modelview;
uniform mat4 screenTransformation;
uniform mat4 windowTransformation;

in vec4 vertex;
in vec2 texCoord;

out vec2 varyingTexCoords;

void main()
{
    varyingTexCoords = texCoord;
    gl_Position = projection * (modelview * screenTransformation * windowTransformation) * vertex;
}

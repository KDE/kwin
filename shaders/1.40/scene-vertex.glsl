#version 140

uniform mat4 projection;

// offset of the window/texture to be rendered
uniform vec2 offset;

in vec4 vertex;
in vec2 texCoord;

out vec2 varyingTexCoords;

void main()
{
    varyingTexCoords = texCoord;
    gl_Position = projection * vec4(vertex.xy + offset, vertex.zw);
}

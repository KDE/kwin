uniform mat4 projection;
// offset of the window/texture to be rendered
uniform vec2 offset;

uniform float textureWidth;
uniform float textureHeight;

// passed in vertex - only x and y are used
attribute vec4 vertex;
// passed in texCoords - to be forwarded
attribute vec2 texCoord;

// texCoords passed to fragment shader
varying vec2 varyingTexCoords;

void main() {
    varyingTexCoords = texCoord / vec2(textureWidth, textureHeight);
    gl_Position = projection*vec4(vertex.xy + offset, vertex.zw);
}

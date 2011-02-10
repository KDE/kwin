uniform mat4 projection;
uniform mat4 modelview;
uniform mat4 screenTransformation;
uniform mat4 windowTransformation;

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
    gl_Position = vertex*(windowTransformation*screenTransformation*modelview)*projection;
}

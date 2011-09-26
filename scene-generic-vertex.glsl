uniform mat4 projection;
uniform mat4 modelview;
uniform mat4 screenTransformation;
uniform mat4 windowTransformation;

// passed in vertex - only x and y are used
attribute vec4 vertex;
// passed in texCoords - to be forwarded
attribute vec2 texCoord;

// texCoords passed to fragment shader
varying vec2 varyingTexCoords;

void main() {
    varyingTexCoords = texCoord;
    gl_Position = projection*(modelview*screenTransformation*windowTransformation)*vertex;
}

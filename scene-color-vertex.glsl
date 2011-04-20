uniform mat4 projection;
// offset of the window/texture to be rendered
uniform vec2 offset;

// passed in vertex - only x and y are used
attribute vec4 vertex;

void main() {
    gl_Position = projection*vec4(vertex.xy + offset, vertex.zw);
}

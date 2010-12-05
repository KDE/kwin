#ifdef GL_ES
precision highp float;
#endif
// size of the complete display in pixels, x==width, y==height
uniform vec2 displaySize;
// geometry of the window/texture to be rendered: x, y, width, height in display geometry
uniform vec4 geometry;

// passed in vertex - only x and y are used
attribute vec4 vertex;

void main() {
    gl_Position.xy = 2.0*vec2(geometry.x + vertex.x, displaySize.y - vertex.y - geometry.y)/displaySize - vertex.ww;
}

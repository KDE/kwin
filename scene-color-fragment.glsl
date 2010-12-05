#ifdef GL_ES
precision highp float;
#endif
uniform vec4 geometryColor;

// not used
varying vec2 varyingTexCoords;

void main() {
    gl_FragColor = geometryColor;
}

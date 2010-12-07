#ifdef GL_ES
precision highp float;
#endif
uniform vec4 geometryColor;

void main() {
    gl_FragColor = geometryColor;
}

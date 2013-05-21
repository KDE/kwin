uniform float u_alpha;

varying vec2 varyingTexCoords;

void main()
{
    gl_FragColor = vec4(0.0, 0.0, 0.0, u_alpha*varyingTexCoords.s);
}

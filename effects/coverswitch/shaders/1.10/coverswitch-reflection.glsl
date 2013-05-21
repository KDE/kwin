uniform vec4 u_frontColor;
uniform vec4 u_backColor;

varying vec2 varyingTexCoords;

void main()
{
    gl_FragColor = u_frontColor*(1.0-varyingTexCoords.s) + u_backColor*varyingTexCoords.s;
}

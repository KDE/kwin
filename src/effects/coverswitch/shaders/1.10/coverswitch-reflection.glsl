uniform vec4 u_frontColor;
uniform vec4 u_backColor;

varying vec2 texcoord0;

void main()
{
    gl_FragColor = u_frontColor*(1.0-texcoord0.s) + u_backColor*texcoord0.s;
}

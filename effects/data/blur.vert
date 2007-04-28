varying vec2 pos;
varying vec2 blurDirection;

attribute float xBlur;
attribute float yBlur;


void main()
{
    blurDirection = vec2(xBlur, yBlur);
    pos = gl_Vertex.xy;
    gl_Position = ftransform();
}

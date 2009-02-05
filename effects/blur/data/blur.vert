varying vec2 samplePos1;
varying vec2 samplePos2;
varying vec2 samplePos3;
varying vec2 samplePos4;
varying vec2 samplePos5;

uniform float textureWidth;
uniform float textureHeight;
attribute float xBlur;
attribute float yBlur;


vec2 mkSamplePos(vec2 origin, float offset)
{
    vec2 foo = origin + vec2(xBlur, yBlur) * offset;
    return vec2(foo.x / textureWidth, 1.0 - foo.y / textureHeight);
}

void main()
{
    samplePos1 = mkSamplePos(gl_Vertex.xy,  0.0);
    samplePos2 = mkSamplePos(gl_Vertex.xy, -1.5);
    samplePos3 = mkSamplePos(gl_Vertex.xy,  1.5);
    samplePos4 = mkSamplePos(gl_Vertex.xy,  3.5);
    samplePos5 = mkSamplePos(gl_Vertex.xy, -3.5);

    gl_Position = ftransform();
}

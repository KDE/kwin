void main()
{
    gl_TexCoord[0].xy = gl_Vertex.xy;
    gl_Position = ftransform();
}


uniform sampler2D winTexture;
uniform float windowWidth;
uniform float windowHeight;
uniform float opacity;
uniform float front;
uniform float useTexture;

vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / windowWidth, pix.y / windowHeight);
}

void main()
{
    if( front > 0.0 && gl_FrontFacing )
        discard;
    if( front < 0.0 && !gl_FrontFacing )
        discard;

    if( useTexture > 0.0 )
        {
        // remove the shadow decoration quads
        if( gl_TexCoord[0].x < 0.0 || gl_TexCoord[0].x > windowWidth ||
            gl_TexCoord[0].y < 0.0 || gl_TexCoord[0].y > windowHeight )
            discard;
        gl_FragColor.rgba = texture2D(winTexture, pix2tex(gl_TexCoord[0].xy)).rgba;
        }
    else
        {
        gl_FragColor = gl_Color;
        }
    gl_FragColor.a = gl_FragColor.a * opacity;
}

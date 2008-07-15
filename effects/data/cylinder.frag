uniform sampler2D winTexture;
uniform float windowWidth;
uniform float windowHeight;
uniform float opacity;

vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / windowWidth, pix.y / windowHeight);
}

void main()
{
    gl_FragColor.rgba = texture2D(winTexture, pix2tex(gl_TexCoord[0].xy)).rgba;
    gl_FragColor.a *= opacity;
}

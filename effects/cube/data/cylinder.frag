uniform sampler2D winTexture;
uniform float textureWidth;
uniform float textureHeight;
uniform float opacity;
uniform float brightness;
uniform float saturation;
uniform float front;
uniform float useTexture;

vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / textureWidth, pix.y / textureHeight);
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
        if( gl_TexCoord[0].x < 0.0 || gl_TexCoord[0].x > textureWidth ||
            gl_TexCoord[0].y < 0.0 || gl_TexCoord[0].y > textureHeight )
            discard;
        vec4 tex = texture2D(winTexture, pix2tex(gl_TexCoord[0].xy));
        tex = vec4( tex.rgb, tex.a * opacity );
        if( saturation != 1.0 )
            {
            vec3 desaturated = tex.rgb * vec3( 0.30, 0.59, 0.11 );
            desaturated = vec3( dot( desaturated, tex.rgb ));
            tex.rgb = tex.rgb * vec3( saturation ) + desaturated * vec3( 1.0 - saturation );
            }
        tex.rgb = tex.rgb * vec3( brightness );
        gl_FragColor = tex;
        }
    else
        {
        gl_FragColor = gl_Color;
        }
}

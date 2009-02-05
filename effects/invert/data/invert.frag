uniform sampler2D winTexture;
uniform float textureWidth;
uniform float textureHeight;
uniform float opacity;
uniform float brightness;
uniform float saturation;

// Converts pixel coordinates to texture coordinates
vec2 pix2tex( vec2 pix )
{
    return vec2( pix.s / textureWidth, pix.t / textureHeight );
}

void main()
{
    vec4 tex = texture2D( winTexture, pix2tex( gl_TexCoord[0].st ));
    tex = vec4( vec3( 1.0 ) - tex.rgb, tex.a * opacity );
    if( saturation != 1.0 )
        {
        vec3 desaturated = tex.rgb * vec3( 0.30, 0.59, 0.11 );
        desaturated = vec3( dot( desaturated, tex.rgb ));
        tex.rgb = tex.rgb * vec3( saturation ) + desaturated * vec3( 1.0 - saturation );
        }
    tex.rgb = tex.rgb * vec3( brightness );

    gl_FragColor = tex;
}

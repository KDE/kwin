uniform sampler2D sceneTex;
uniform float textureWidth;
uniform float textureHeight;


// Converts pixel coordinates to texture coordinates
vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / textureWidth, 1.0 - pix.y / textureHeight);
}

void main()
{
    vec3 tex = texture2D(sceneTex, pix2tex(gl_TexCoord[0].xy)).rgb;

    gl_FragColor = vec4(vec3(1.0) - tex, 1.0);
}


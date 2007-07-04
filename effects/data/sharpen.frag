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
    float amount = 0.4;
    vec3 tex = texture2D(sceneTex, pix2tex(gl_TexCoord[0].xy)).rgb * (1.0 + 4.0 * amount);

    tex -= texture2D(sceneTex, pix2tex(gl_TexCoord[0].xy + vec2(0.0, 1.0))).rgb * amount;
    tex -= texture2D(sceneTex, pix2tex(gl_TexCoord[0].xy + vec2(0.0, -1.0))).rgb * amount;
    tex -= texture2D(sceneTex, pix2tex(gl_TexCoord[0].xy + vec2(1.0, 0.0))).rgb * amount;
    tex -= texture2D(sceneTex, pix2tex(gl_TexCoord[0].xy + vec2(-1.0, 0.0))).rgb * amount;

    gl_FragColor = vec4(tex, 1.0);
}


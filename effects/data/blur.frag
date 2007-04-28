uniform sampler2D inputTex;
uniform float displayWidth;
uniform float displayHeight;

varying vec2 pos;
varying vec2 blurDirection;


// Converts pixel coordinates to texture coordinates
vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / displayWidth, 1.0 - pix.y / displayHeight);
}

vec3 blurTex(float offset, float strength)
{
    return texture2D(inputTex, pix2tex(pos + blurDirection * offset)).rgb * strength;
}

void main()
{
    // Do the gaussian blur
    // This blur actually has a radius of 4, but we take advantage of gpu's
    //  linear texture filtering, so e.g. 1.5 actually gives us both texels
    //  1 and 2
    vec3 tex = blurTex(0.0, 0.20);
    tex += blurTex(-1.5, 0.30);
    tex += blurTex( 1.5, 0.30);
    tex += blurTex(-3.5, 0.10);
    tex += blurTex( 3.5, 0.10);

    gl_FragColor = vec4(tex, 1.0);
}


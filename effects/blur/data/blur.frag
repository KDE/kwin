uniform sampler2D inputTex;

varying vec2 samplePos1;
varying vec2 samplePos2;
varying vec2 samplePos3;
varying vec2 samplePos4;
varying vec2 samplePos5;


// If defined, use five samples (blur radius = 5), otherwise 3 samples (radius = 3)
#define FIVE_SAMPLES


vec3 blurTex(vec2 pos, float strength)
{
    return texture2D(inputTex, pos).rgb * strength;
}

void main()
{
    // Do the gaussian blur
    // This blur actually has a radius of 4, but we take advantage of gpu's
    //  linear texture filtering, so e.g. 1.5 actually gives us both texels
    //  1 and 2
#ifdef FIVE_SAMPLES
    vec3 tex = blurTex(samplePos1, 0.30);
    tex += blurTex(samplePos2, 0.25);
    tex += blurTex(samplePos3, 0.25);
    tex += blurTex(samplePos4, 0.1);
    tex += blurTex(samplePos5, 0.1);
#else
    vec3 tex = blurTex(samplePos1, 0.40);
    tex += blurTex(samplePos2, 0.30);
    tex += blurTex(samplePos3, 0.30);
#endif

    gl_FragColor = vec4(tex, 1.0);
}


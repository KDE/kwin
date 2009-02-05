uniform sampler2D winTexture;
uniform sampler2D startOffsetTexture;
uniform sampler2D endOffsetTexture;
uniform float factor;
uniform float scale;

const float regionTexSize = 512.0;


vec2 getOffset(sampler2D texture, vec2 pos)
{
    return (texture2D(texture, pos / regionTexSize).xy - 0.5) / (5.0 / 256.0);
}

void main()
{
    // Original (unscaled) position in pixels
    vec2 origpos = gl_TexCoord[0].xy;
    // Position in pixels on the scaled window
    vec2 pos = origpos * scale;
    // Start/end position of current region
    vec2 rstart = origpos + getOffset(startOffsetTexture, origpos);
    vec2 rend = origpos + getOffset(endOffsetTexture, origpos);
    float alpha = texture2D(startOffsetTexture, origpos / regionTexSize).b;
    // Distance from the start of the region
    vec2 dist = pos - rstart*scale;
    if(any(greaterThan(dist, rend-rstart)))
        discard;//alpha = 0.0;

    vec4 transformedtexcoord = vec4(rstart + dist, vec2(1.0)) * gl_TextureMatrix[0];
    vec3 tex = texture2D(winTexture, transformedtexcoord.xy).rgb;
#if 0
    // ATM we ignore custom opacity values because Fade effect fades out the
    //  window which results in the explosion being way too quick. Once there's
    //  a way to suppress Fade effect when ExplosionEffect is active, we can
    //  use the custom opacity again
    gl_FragColor = vec4(tex, (1.0 - factor*factor) * alpha * opacity);
#else
    gl_FragColor = vec4(tex, (1.0 - factor*factor) * alpha);
#endif
}


uniform sampler2D windowTex;
uniform sampler2D backgroundTex;
uniform float displayWidth;
uniform float displayHeight;
uniform float opacity;
uniform float saturation;
uniform float brightness;


// Converts pixel coordinates to texture coordinates
vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / displayWidth, pix.y / displayHeight);
}

// Returns color of the window at given texture coordinate, taking into
//  account opacity, brightness and saturation
vec4 windowColor(vec2 texcoord)
{
    vec3 color = texture2D(windowTex, texcoord).rgb;
    // Apply saturation
    float grayscale = dot(vec3(0.30, 0.59, 0.11), color.rgb);
    color = mix(vec3(grayscale), color, saturation);
    // Apply brightness
    color = color * brightness;
    // Apply opacity and return
    return vec4(color, opacity);
}

void main()
{
    vec2 texcoord = (gl_TexCoord[0] * gl_TextureMatrix[0]).xy;
    vec2 blurtexcoord = pix2tex(gl_FragCoord.xy); //(gl_FragCoord * gl_TextureMatrix[4]).xy;
    vec3 tex = mix(texture2D(backgroundTex, blurtexcoord).rgb,
                   windowColor(texcoord).rgb, opacity);

    gl_FragColor = vec4(tex, 1.0);
}


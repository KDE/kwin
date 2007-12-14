uniform sampler2D windowTex;
uniform sampler2D backgroundTex;
uniform float textureWidth;
uniform float textureHeight;
uniform float opacity;
uniform float saturation;
uniform float brightness;


// Converts pixel coordinates to texture coordinates
vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / textureWidth, pix.y / textureHeight);
}

// Returns color of the window at given texture coordinate, taking into
//  account brightness and saturation, but not opacity
vec4 windowColor(vec2 texcoord)
{
    vec4 color = texture2D(windowTex, texcoord);
    // Apply saturation
    float grayscale = dot(vec3(0.30, 0.59, 0.11), color.rgb);
    color.rgb = mix(vec3(grayscale), color.rgb, saturation);
    // Apply brightness
    color.rgb = color.rgb * brightness;
    // and return
    return color;
}

void main()
{
    vec2 blurtexcoord = pix2tex(gl_FragCoord.xy);

    vec4 winColor = windowColor(gl_TexCoord[0].xy);
    vec3 tex = mix(texture2D(backgroundTex, blurtexcoord).rgb,
                   winColor.rgb, winColor.a * opacity);

    gl_FragColor = vec4(tex, pow(winColor.a, 0.2));
}


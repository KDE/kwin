uniform sampler2D sampler;
uniform vec4 modulation;
uniform float saturation;
uniform int u_forceAlpha;

varying vec2 varyingTexCoords;

//varying vec4 color;

void main() {
    vec4 tex = texture2D(sampler, varyingTexCoords);

    tex.a = min(tex.a + float(u_forceAlpha), 1.0);

    if (saturation != 1.0) {
        tex.rgb = mix(vec3(dot( vec3( 0.30, 0.59, 0.11 ), tex.rgb )), tex.rgb, saturation);
    }

    tex *= modulation;

#ifdef KWIN_SHADER_DEBUG
    tex.g = min(tex.g + 0.5, 1.0);
#endif

    gl_FragColor = tex;
}

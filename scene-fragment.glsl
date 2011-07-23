uniform sampler2D sampler;
uniform vec4 modulation;
uniform float saturation;
uniform int u_forceAlpha;

varying vec2 varyingTexCoords;

//varying vec4 color;

void main() {
    vec4 tex = texture2D(sampler, varyingTexCoords);

    if (u_forceAlpha > 0) {
        tex.a = 1.0;
    }

    if( saturation != 1.0 ) {
        vec3 desaturated = tex.rgb * vec3( 0.30, 0.59, 0.11 );
        desaturated = vec3( dot( desaturated, tex.rgb ));
        tex.rgb = tex.rgb * vec3( saturation ) + desaturated * vec3( 1.0 - saturation );
    }

    tex *= modulation;

#ifdef KWIN_SHADER_DEBUG
    tex.g = min(tex.g + 0.5, 1.0);
#endif

    gl_FragColor = tex;
}

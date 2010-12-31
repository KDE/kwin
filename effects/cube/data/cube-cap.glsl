uniform sampler2D sample;
uniform vec4 u_capColor;
uniform float u_opacity;

varying vec2 varyingTexCoords;

void main() {
    vec4 color = u_capColor;
    vec4 tex = texture2D(sample, varyingTexCoords);
    if (varyingTexCoords.s < 0.0 || varyingTexCoords.s > 1.0 ||
            varyingTexCoords.t < 0.0 || varyingTexCoords.t > 1.0) {
        tex = u_capColor;
    }
    color.rgb = tex.rgb*tex.a + color.rgb*(1.0-tex.a);
    color.a = u_opacity;

    gl_FragColor = color;
}

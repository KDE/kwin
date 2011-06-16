uniform sampler2D sampler;
uniform vec4 u_color;

varying vec2 varyingTexCoords;

void main()
{
    vec4 tex = texture2D(sampler, varyingTexCoords);
    if (tex.a != 1.0) {
        tex = u_color;
    }
    gl_FragColor = tex;
}

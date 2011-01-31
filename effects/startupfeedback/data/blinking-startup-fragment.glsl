uniform sampler2D sample;
uniform float textureWidth;
uniform float textureHeight;
uniform vec4 u_color;

varying vec2 varyingTexCoords;

void main()
{
    vec4 tex = texture2D(sample, varyingTexCoords);
    if (tex.a != 1.0) {
        tex = u_color;
    }
    gl_FragColor = tex;
}

uniform sampler2D texUnit;
uniform mat4 colorMatrix;
uniform float opacity;

varying vec2 uv;

void main(void)
{
    vec4 tex = texture2D(texUnit, uv);

    if (opacity >= 1.0) {
        gl_FragColor = tex * colorMatrix;
    } else {
        gl_FragColor = tex * (opacity * colorMatrix + (1.0 - opacity) * mat4(1.0));
    }
}

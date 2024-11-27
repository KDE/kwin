uniform sampler2D texUnit;
uniform mat4 colorMatrix;

varying vec2 uv;

void main(void)
{
    vec4 tex = texture2D(texUnit, uv);

    gl_FragColor = tex * colorMatrix;
}

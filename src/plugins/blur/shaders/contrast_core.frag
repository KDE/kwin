#version 140

uniform sampler2D texUnit;
uniform mat4 colorMatrix;
uniform float opacity;

in vec2 uv;

out vec4 fragColor;

void main(void)
{
    vec4 tex = texture(texUnit, uv);

    if (opacity >= 1.0) {
        fragColor = tex * colorMatrix;
    } else {
        fragColor = tex * (opacity * colorMatrix + (1.0 - opacity) * mat4(1.0));
    }
}

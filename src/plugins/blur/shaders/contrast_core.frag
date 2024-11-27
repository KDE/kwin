#version 140

uniform sampler2D texUnit;
uniform mat4 colorMatrix;

in vec2 uv;

out vec4 fragColor;

void main(void)
{
    vec4 tex = texture(texUnit, uv);

    fragColor = tex * colorMatrix;
}

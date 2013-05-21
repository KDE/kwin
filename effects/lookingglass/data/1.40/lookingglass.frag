#version 140
uniform sampler2D sampler;
uniform vec2 u_cursor;
uniform float u_zoom;
uniform float u_radius;
uniform vec2 u_textureSize;

in vec2 varyingTexCoords;

out vec4 fragColor;

#define PI 3.14159

void main()
{
    vec2 d = u_cursor - varyingTexCoords;
    float dist = sqrt(d.x*d.x + d.y*d.y);
    vec2 texcoord = varyingTexCoords;
    if (dist < u_radius) {
        float disp = sin(dist / u_radius * PI) * (u_zoom - 1.0) * 20.0;
        texcoord += d / dist * disp;
    }

    texcoord = texcoord/u_textureSize;
    texcoord.t = 1.0 - texcoord.t;
    fragColor = texture(sampler, texcoord);
}


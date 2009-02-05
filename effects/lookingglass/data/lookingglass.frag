uniform sampler2D sceneTex;
uniform float textureWidth;
uniform float textureHeight;
uniform float cursorX;
uniform float cursorY;
uniform float zoom;
uniform float radius;

#define PI 3.14159

// Converts pixel coordinates to texture coordinates
vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / textureWidth, 1.0 - pix.y / textureHeight);
}

void main()
{
    vec2 texcoord = gl_TexCoord[0].xy;

    float dx = cursorX - texcoord.x;
    float dy = cursorY - texcoord.y;
    float dist = sqrt(dx*dx + dy*dy);
    if(dist < radius)
    {
        float disp = sin(dist / radius * PI) * (zoom - 1.0) * 20.0;
        texcoord.x += dx / dist * disp;
        texcoord.y += dy / dist * disp;
    }

    gl_FragColor = texture2D(sceneTex, pix2tex(texcoord));
}


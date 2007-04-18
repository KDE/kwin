uniform sampler2D sceneTex;
uniform float displayWidth;
uniform float displayHeight;
uniform float time;

varying vec2 pos;


#define DEG2RAD (6.2832/360.0)


// Converts pixel coordinates to texture coordinates
vec2 pix2tex(vec2 pix)
{
    return vec2(pix.x / displayWidth, 1.0 - pix.y / displayHeight);
}

float wave(vec2 pos, vec2 wave_dir, float wave_length, float wave_speed)
{
    return sin(((pos.x * wave_dir.x) + (pos.y * wave_dir.y)) * 6.2832 / wave_length +
            time * wave_speed * 6.2832 / wave_length);
}

vec2 displacement(float wave_angle, float wave_length, float wave_speed, float wave_height)
{
    vec2 wave_dir = vec2(cos(wave_angle * DEG2RAD), sin(wave_angle * DEG2RAD));
    return wave_dir * (wave(pos, wave_dir, wave_length, wave_speed) * wave_height);
}

// Only for debugging
float wave_color(float wave_angle, float wave_length, float wave_speed, float wave_height)
{
    vec2 wave_dir = vec2(cos(wave_angle * DEG2RAD), sin(wave_angle * DEG2RAD));
    return wave(pos, wave_dir, wave_length, wave_speed) * 0.5 + 0.5;
}

void main()
{
    vec2 texpos = pos;
    texpos += displacement( 20.0, 250.0, 120.0, 4.0);
    texpos += displacement(-40.0, 350.0, 100.0, 4.0);
    texpos += displacement(240.0, 1000.0, 100.0, 12.0);
    texpos += displacement(160.0, 50.0, 30.0, 2.0);

    vec3 tex = texture2D(sceneTex, pix2tex(texpos)).rgb;
    //tex.r = wave_color( 20, 250, 150, 4);  // debug

    gl_FragColor = vec4(tex, 1.0);
}


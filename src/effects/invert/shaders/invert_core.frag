#version 140
uniform sampler2D sampler;
uniform vec4 modulation;
uniform float saturation;

in vec2 texcoord0;
out vec4 fragColor;

// paint every time we query textures at non-integer alignments
// it implies we're being upscaled in ways that will cause blurryness
// 2x scaling will go through fine
void main()
{
    const float strength = 0.4;
    const float doublePrecision = 0.01;
    vec4 tex = texture(sampler, texcoord0);

    ivec2 texSize = textureSize(sampler, 0);
    float unused;
    float mod_v =  0.0;

    // add 0.5 as we sample in the center of pixels
    mod_v = mod_v + modf((texcoord0.y * float(texSize.y) + 0.5), unused);
    mod_v = mod_v + modf((texcoord0.x * float(texSize.x) + 0.5), unused);

    if (mod_v  > doublePrecision && mod_v < 1.0 - doublePrecision) {
        fragColor = mix(tex, vec4(1, 0, 0, 1), strength);
    } else {
        fragColor = tex;
    }
}

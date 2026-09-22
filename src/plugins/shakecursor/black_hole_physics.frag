#version 140

uniform vec3 blackHolePosition;
uniform sampler2D sampler;
uniform vec2 size;
uniform float diameter;
uniform float timestep;

in vec2 texcoord0;
out vec4 fragColor;

void main()
{
    vec2 absoluteTexCoord = texcoord0 * size;

    // xy: source coordinate (normalized)
    // z: swallowed already, or not
    // w: speed
    vec4 data = texelFetch(sampler, ivec2(absoluteTexCoord), 0);

    vec2 delta = blackHolePosition.xy - data.xy;
    float dist = max(length(delta), 0.01);

    // find the next pixel that will occupy this space
    // -> ðxy pixels next to the current one
    vec2 velocity = normalize(delta) * pow(diameter, 3.0) * pow(1.0 / dist, 2.0);
    data.xy -= velocity * timestep;
//     dakta.z *= step(diameter, dist);
    data.w = length(velocity);

    fragColor = data;
}

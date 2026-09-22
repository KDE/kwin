#version 140
#include "colormanagement.glsl"

uniform vec3 blackHolePosition;
uniform vec2 size;
uniform float diameter;
uniform float whiteHoleDiameter;

uniform sampler2D screen;
uniform sampler2D positions;

in vec2 texcoord0;
out vec4 fragColor;

void main()
{
    vec2 delta = blackHolePosition.xy - texcoord0 * size;
    float dist = length(delta);

    if (whiteHoleDiameter == 0.0) {
        // maximum lensing is at dist = diameter
        float lensingStrength = diameter * pow(10.0 / (dist - diameter), 2.0);

        // move texcoord towards center proportional to 1/dist
        // to mimic gravitational lensing
        vec2 lensing = normalize(delta) * lensingStrength;

        vec4 position = texture(positions, texcoord0);
        vec4 tex = texture(screen, (position.xy + lensing) / size);

        tex = sourceEncodingToNitsInDestinationColorspace(tex);
        tex.rgb *= step(diameter, dist);
        fragColor = nitsToDestinationEncoding(tex);
    } else {
        vec4 tex = texture(screen, texcoord0);
        tex = sourceEncodingToNitsInDestinationColorspace(tex);

        tex.rgb *= 1.0 - step(whiteHoleDiameter, dist);
//         float mult = step(whiteHoleDiameter - 10.0, dist);
//         tex.rgb = tex.rgb * (1.0 - mult) + vec3(mult);

        fragColor = nitsToDestinationEncoding(tex);
    }
}

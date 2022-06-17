/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

uniform sampler2D sampler;
uniform float fractionalPrecision;
uniform vec2 texSize;

varying vec2 texcoord0;
varying float vertexFractional;

// paint every time we query textures at non-integer alignments
// it implies we're being upscaled in ways that will cause blurryness
// 2x scaling will go through fine
void main()
{
    const float strength = 0.4;

    vec4 tex = texture(sampler, texcoord0);

    vec2 sourcePixel = texcoord0 * texSize;
    vec2 destinationPixel = gl_FragCoord.xy;

    // This adds a small value because otherwise we have some sort of loss of
    // precision that causes glitching.
    vec2 error = fract((sourcePixel - destinationPixel) + vec2(0.0005));

    fragColor = tex;

    if (vertexFractional > 0.5) {
        fragColor = mix(fragColor, vec4(0.0, 0.0, 1.0, 1.0), strength);
    }

    if (error.x > fractionalPrecision || error.y > fractionalPrecision) {
        fragColor = mix(fragColor, vec4(1.0, 0.0, 0.0, 1.0), strength);
    }
}

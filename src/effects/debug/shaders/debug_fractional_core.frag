/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#version 140

uniform sampler2D sampler;
uniform float fractionalPrecision;
uniform vec4 modulation;

in vec2 texcoord0;
in float vertexFractional;

out vec4 fragColor;

// paint every time we query textures at non-integer alignments
// it implies we're being upscaled in ways that will cause blurryness
// 2x scaling will go through fine
void main()
{
    const float strength = 0.4;

    // Calculate an error correction value based on the minimum precision we
    // want to measure.
    float errorCorrection = 1.0 / fractionalPrecision;

    vec4 tex = texture(sampler, texcoord0);
    ivec2 texSize = textureSize(sampler, 0);

    // Determine which exact pixel we are reading from the source texture.
    // Texture sampling happens in the middle of a pixel so we need to add 0.5.
    vec2 sourcePixel = texcoord0 * texSize + 0.5;
    // Cancel out any precision artifacts below what we actually want to measure.
    sourcePixel = round(sourcePixel * errorCorrection) / errorCorrection;

    // The total error is the sum of the fractional parts of the source pixel.
    float error = dot(fract(sourcePixel), vec2(1.0));

    tex *= modulation;
    tex.rgb *= tex.a;
    fragColor = tex;

    if (vertexFractional > 0.5) {
        fragColor = mix(fragColor, vec4(0.0, 0.0, 1.0, 1.0), strength);
    }

    if (error > fractionalPrecision) {
        fragColor = mix(fragColor, vec4(1.0, 0.0, 0.0, 1.0), strength);
    }
}


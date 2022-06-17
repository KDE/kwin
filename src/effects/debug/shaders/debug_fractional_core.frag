/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#version 140

uniform sampler2D sampler;
uniform float fractionalPrecision;

in vec2 texcoord0;
in float vertexFractional;

out vec4 fragColor;

// paint every time we query textures at non-integer alignments
// it implies we're being upscaled in ways that will cause blurryness
// 2x scaling will go through fine
void main()
{
    const float strength = 0.4;

    vec4 tex = texture(sampler, texcoord0);
    ivec2 texSize = textureSize(sampler, 0);

    vec2 sourcePixel = texcoord0 * texSize + 0.5;
    float error = dot(fract(sourcePixel), vec2(1.0));

    fragColor = tex;

    if (vertexFractional > 0.5) {
        fragColor = mix(fragColor, vec4(0.0, 0.0, 1.0, 1.0), strength);
    }

    if (error > fractionalPrecision) {
        fragColor = mix(fragColor, vec4(1.0, 0.0, 0.0, 1.0), strength);
    }
}

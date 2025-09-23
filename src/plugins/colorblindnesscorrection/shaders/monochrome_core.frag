#version 140
// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: CC0-1.0
#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
in vec2 texcoord0;
out vec4 fragColor;

void main()
{
    vec4 tex = texture2D(sampler, texcoord0);
    tex = sourceEncodingToNitsInDestinationColorspace(tex);

    tex = adjustSaturation(tex);

    gl_FragColor = nitsToDestinationEncoding(tex);
}

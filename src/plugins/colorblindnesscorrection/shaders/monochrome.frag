// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: CC0-1.0
#include "colormanagement.glsl"
#include "saturation.glsl"

uniform sampler2D sampler;
varying vec2 texcoord0;

void main()
{
    vec4 tex = texture2D(sampler, texcoord0);
    tex = sourceEncodingToNitsInDestinationColorspace(tex);

    tex = adjustSaturation(tex);

    gl_FragColor = nitsToDestinationEncoding(tex);
}

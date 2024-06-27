#version 140
// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: CC0-1.0
#include "saturation.glsl"
#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;
uniform float intensity;
in vec2 texcoord0;
out vec4 fragColor;
uniform mat3 defectMatrix;

const mat3 srgbToLMS = mat3(
    17.8824, 3.45565, 0.0299566,
    43.5161, 27.1554, 0.184309,
    4.11935, 3.86714, 1.46709
);
const mat3 errorMat = mat3(
    0.0809444479, -0.0102485335, -0.000365296938,
   -0.130504409,   0.0540193266, -0.00412161469,
    0.116721066,  -0.113614708,   0.693511405
);

void main()
{
    vec4 tex = texture2D(sampler, texcoord0);
    tex = sourceEncodingToNitsInDestinationColorspace(tex);
    tex = adjustSaturation(tex);

    vec3 LMS = srgbToLMS * tex.rgb;
    vec3 lms = defectMatrix * LMS;
    vec3 error = errorMat * lms;

    vec3 diff = (tex.rgb - error) * vec3(intensity);
    vec3 correction = vec3(0.0,
                           (diff.r * 0.7) + (diff.g * 1.0),
                           (diff.r * 0.7) + (diff.b * 1.0));

    tex = (tex + vec4(correction, 0.0)) * modulation;
    gl_FragColor = nitsToDestinationEncoding(tex);
}

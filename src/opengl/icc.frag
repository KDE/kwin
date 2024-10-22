// SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "colormanagement.glsl"

precision highp float;
precision highp sampler2D;
precision highp sampler3D;

in vec2 texcoord0;

uniform sampler2D src;

uniform mat4 toXYZD50;

uniform int Bsize;
uniform sampler2D Bsampler;

uniform mat4 matrix2;

uniform int Msize;
uniform sampler2D Msamplrt;

uniform ivec3 Csize;
uniform sampler3D Csampler;

uniform int Asize;
uniform sampler2D Asampler;

vec3 sample1DLut(vec3 input, sampler2D lut, int lutSize) {
    float lutOffset = 0.5 / float(lutSize);
    float lutScale = 1.0 - lutOffset * 2.0;
    float lutR = texture2D(lut, vec2(lutOffset + input.r * lutScale, 0.5)).r;
    float lutG = texture2D(lut, vec2(lutOffset + input.g * lutScale, 0.5)).g;
    float lutB = texture2D(lut, vec2(lutOffset + input.b * lutScale, 0.5)).b;
    return vec3(lutR, lutG, lutB);
}

void main()
{
    vec4 tex = texture2D(src, texcoord0);
    tex = encodingToNits(tex, sourceNamedTransferFunction, sourceTransferFunctionParams.x, sourceTransferFunctionParams.y);
    tex.rgb /= max(tex.a, 0.001);
    tex.rgb /= destinationReferenceLuminance;
    tex.rgb = (toXYZD50 * vec4(tex.rgb, 1.0)).rgb;
    if (Bsize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Bsampler, Bsize);
    }
    tex.rgb = (matrix2 * vec4(tex.rgb, 1.0)).rgb;
    if (Msize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Msampler, Msize);
    }
    if (Csize > 0) {
        vec3 lutOffset = vec3(0.5) / vec3(Csize);
        vec3 lutScale = vec3(1.0) - lutOffset * 2.0;
        tex.rgb = texture3D(Csampler, lutOffset + tex.rgb * lutScale).rgb;
    }
    if (Asize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Asampler, Asize);
    }
    tex.rgb *= tex.a;
    gl_FragColor = tex;
}

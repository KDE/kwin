#version 140
// SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
precision highp float;

in vec2 texcoord0;

out vec4 fragColor;

uniform sampler2D src;
uniform float sdrBrightness;

uniform mat3 matrix1;

uniform int Bsize;
uniform sampler1D Bsampler;

uniform mat4 matrix2;

uniform int Msize;
uniform sampler1D Msampler;

uniform ivec3 Csize;
uniform sampler3D Csampler;

uniform int Asize;
uniform sampler1D Asampler;

vec3 sample1DLut(in vec3 srcColor, in sampler1D lut, in int lutSize) {
    float lutOffset = 0.5 / lutSize;
    float lutScale = 1 - lutOffset * 2;
    float lutR = texture(lut, lutOffset + srcColor.r * lutScale).r;
    float lutG = texture(lut, lutOffset + srcColor.g * lutScale).g;
    float lutB = texture(lut, lutOffset + srcColor.b * lutScale).b;
    return vec3(lutR, lutG, lutB);
}

void main()
{
    vec4 tex = texture(src, texcoord0);
    tex.rgb /= sdrBrightness;
    tex.rgb = matrix1 * tex.rgb;
    if (Bsize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Bsampler, Bsize);
    }
    tex.rgb = (matrix2 * vec4(tex.rgb, 1.0)).rgb;
    if (Msize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Msampler, Msize);
    }
    if (Csize.x > 0) {
        vec3 lutOffset = vec3(0.5) / Csize;
        vec3 lutScale = vec3(1) - lutOffset * 2;
        tex.rgb = texture(Csampler, lutOffset + tex.rgb * lutScale).rgb;
    }
    if (Asize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Asampler, Asize);
    }
    fragColor = tex;
}

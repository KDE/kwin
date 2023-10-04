// SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
precision highp float;

in vec2 texcoord0;

uniform sampler2D src;
uniform float sdrBrightness;

uniform mat3 matrix1;

uniform int Bsize;
uniform sampler1D Bsampler;

uniform mat4 matrix2;

uniform int Msize;
uniform sampler1D Msamplrt;

uniform ivec3 Csize;
uniform sampler3D Csampler;

uniform int Asize;
uniform sampler1D Asampler;

vec3 sample1DLut(vec3 input, sampler1D lut, int lutSize) {
    float lutOffset = 0.5 / lutSize;
    float lutScale = 1 - lutOffset * 2;
    float lutR = texture1D(lut, lutOffset + input.r * lutScale).r;
    float lutG = texture1D(lut, lutOffset + input.g * lutScale).g;
    float lutB = texture1D(lut, lutOffset + input.b * lutScale).b;
    return vec3(lutR, lutG, lutB);
}

void main()
{
    vec4 tex = texture2D(src, texcoord0);
    tex.rgb /= sdrBrightness;
    tex.rgb = matrix1 * tex.rgb;
    if (Bsize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Bsampler, Bsize);
    }
    tex.rgb = (matrix2 * vec4(tex.rgb, 1.0)).rgb;
    if (Msize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Msampler, Msize);
    }
    if (Csize > 0) {
        vec3 lutOffset = vec3(0.5) / Csize;
        vec3 lutScale = vec3(1) - lutOffset * 2;
        tex.rgb = texture3D(Csampler, lutOffset + tex.rgb * lutScale).rgb;
    }
    if (Asize > 0) {
        tex.rgb = sample1DLut(tex.rgb, Asampler, Asize);
    }
    gl_FragColor = tex;
}

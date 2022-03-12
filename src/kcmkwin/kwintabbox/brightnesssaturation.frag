/*
 * SPDX-FileCopyrightText: 2011, 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#version 440

layout (std140, binding = 0) uniform buf {
    highp mat4 u_matrix;
    lowp float u_opacity; // offset 64
    highp float u_saturation; // offset 68
    highp float u_brightness; // offset 72
} ubuf; // size 76

layout (binding = 1) uniform sampler2D qt_Texture;

layout (location = 0) in highp vec2 v_coord;
layout (location = 0) out highp vec4 fragColor;

void main() {
    lowp vec4 tex = texture(qt_Texture, v_coord);
    if (ubuf.u_saturation != 1.0) {
        tex.rgb = mix(vec3(dot( vec3( 0.30, 0.59, 0.11 ), tex.rgb )), tex.rgb, ubuf.u_saturation);
    }
    tex.rgb = tex.rgb * ubuf.u_brightness;
    fragColor = tex * ubuf.u_opacity;
}

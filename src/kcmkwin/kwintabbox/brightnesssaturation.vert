/*
 * SPDX-FileCopyrightText: 2011, 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#version 440

layout (std140, binding = 0) uniform buf {
    highp mat4 u_matrix;
    lowp float u_opacity;
    highp float u_saturation;
    highp float u_brightness;
} ubuf;

layout (location = 0) in highp vec4 vertex;
layout (location = 1) in highp vec2 texCoord;
layout (location = 0) out highp vec2 v_coord;

void main() {
    v_coord = texCoord;
    gl_Position = ubuf.u_matrix * vertex;
}

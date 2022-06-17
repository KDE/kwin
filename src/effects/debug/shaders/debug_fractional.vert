uniform mat4 modelViewProjectionMatrix;
uniform vec2 screenSize;
uniform float fractionalPrecision;
/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

attribute vec4 vertex;
attribute vec4 texcoord;

varying vec2 texcoord0;
varying float vertexFractional;

void main(void)
{
    gl_Position = modelViewProjectionMatrix * vertex;

    vec2 screenPosition = (gl_Position.xy + vec2(1.0) / vec2(2.0)) * screenSize;
    float error = dot(fract(screenPosition), vec2(1.0));
    vertexFractional = error > fractionalPrecision ? 1.0 : 0.0;

    texcoord0 = texcoord.xy;
}

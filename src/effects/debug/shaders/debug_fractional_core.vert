/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#version 140

uniform mat4 modelViewProjectionMatrix;
uniform float fractionalPrecision;
uniform vec2 screenSize;
uniform vec2 geometrySize;

in vec4 vertex;
in vec4 texcoord;

out vec2 texcoord0;
out float vertexFractional;

void main(void)
{
    float errorCorrection = 1.0 / fractionalPrecision;

    gl_Position = modelViewProjectionMatrix * vertex;

    vec2 screenPosition = ((gl_Position.xy / gl_Position.w + vec2(1.0)) / vec2(2.0)) * screenSize;
    // Cancel out any floating point errors below what we want to measure.
    screenPosition = round(screenPosition * errorCorrection) / errorCorrection;

    // Dermine how far off the pixel grid this vertex is.
    vec2 error = fract(screenPosition);

    vertexFractional = dot(error, vec2(1.0)) > fractionalPrecision ? 1.0 : 0.0;

    // Correct texture sampling for floating-point error on the vertices.
    // This currently assumes UV coordinates are always from 0 to 1 over an
    // entire triangle.
    texcoord0 = texcoord.xy + (error / geometrySize);
}

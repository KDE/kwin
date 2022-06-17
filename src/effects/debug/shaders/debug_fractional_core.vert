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

// This shader calculates the fractional component of the vertex position and
// passes 1 to the fragment shader if it is larger than the precision we want to
// measure, or 0 if it is not. The fragment shader can then use that information
// to color the pixel based on that value. 0 or 1 is used instead of something
// like vertex coloring because of vertex interpolation and the fragment shader
// having control over the final appearance.
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

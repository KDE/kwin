/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Ritchie Frodomar <ritchie@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later

    This shader implements the xBRZ pixel upscaling
    algorithm. It can be used to upscale the Plasma desktop
    while keeping text and icons looking reasonably sharp
    at extremely high zoom levels.

    This implementation works within the ICtCp color space
    when determining pixel patterns.
*/

#version 140

#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform int textureWidth;
uniform int textureHeight;
uniform float zoomLevel;

// Blend types determine how regions in the destination image are filled in.
// BLEND_NONE = don't do anything special, just use source color (solid colors, area too noisy, etc.)
// BLEND_NORMAL: this region is part of a diagonal edge
// BLEND_DOMINANT: this region is the corner of a shape
const int BLEND_NONE = 0;
const int BLEND_NORMAL = 1;
const int BLEND_DOMINANT = 2;

// Minimum slope before a shallow line is considered steep
const float STEEPNESS_THRESHOLD = 2.2;

// Multiplier applied to differences in color, affects how much contrast is needed before colors are considered different
const float INTENSITY_WEIGHT =  1.0;

// Minimum difference in color before two colors are considered different
const float COLOR_SIMULARITY_TOLERANCE =  0.2254;

// Multiplier applied to downward gradients during initial pattern recognition.
// // When both downward and upward gradients are similar, this value is used to help break the tie
// and pick a winner.
const float DOMINANT_DIRECTION_THRESHOLD =  3.6;

in vec2 texcoord0;

out vec4 fragColor;

float colorDistance(vec3 a, vec3 b) {
    return length((a - b) / sourceReferenceLuminance) * INTENSITY_WEIGHT;
}

/* check whether two colors are similar enough based on their distance */
bool isColorSimilar(vec3 a, vec3 b) {
    return colorDistance(a, b) < COLOR_SIMULARITY_TOLERANCE;
}

/* based on zoom level, position in the scaled pixel, and a line splitting the pixel in half,
   computes whether we are on the "left" side of that line and should fill in this area */
float scaledFillRatio(vec2 pixelOffset, vec2 splitOrigin, vec2 splitDirection, vec2 scale) {
    vec2 offsetFromSplit = pixelOffset - splitOrigin;
    float distanceAlongSplit = dot(offsetFromSplit, splitDirection) / dot(splitDirection, splitDirection);
    vec2 positionOnSplit = splitDirection * distanceAlongSplit;
    vec2 distanceFromSplit = offsetFromSplit - positionOnSplit;
    vec2 perpendicularDirection = vec2(-splitDirection.y, splitDirection.x);
    float splitSide = sign(dot(offsetFromSplit, perpendicularDirection));
    float scaledDistance = splitSide * length(distanceFromSplit * scale);
    return smoothstep(-sqrt(2) / 2, sqrt(2) / 2, scaledDistance);
}

vec3 sampleLinear(in vec2 uv)
{
    return sourceEncodingToNitsInDestinationColorspace(texture(sampler, uv)).rgb;
}

void main()
{
    vec2 textureSize = vec2(textureWidth, textureHeight);
    vec2 pixelSize = 1 / textureSize;
    vec2 scale = vec2(zoomLevel);
    vec2 pixelOffset = (fract(texcoord0 * textureSize) - 0.5);
    vec2 pixelCoord = texcoord0 - pixelOffset * pixelSize;
    vec2 texelOffset = pixelOffset * pixelSize;

    // Each sampled pixel gets a name with a letter representing its row
    // and a number representing its column.
    //
    // Outer pixels are sampled as-needed.
    //
    //  xx  A1  A2  A3  xx
    //  B0  B1  B2  B3  B4
    //  C0  C1  C2  C3  C4
    //  D0  D1  D2  D3  D4
    //  xx  E1  E2  E3  xx

    vec3 B1 = sampleLinear(pixelCoord + pixelSize * vec2(-1, -1));
    vec3 B2 = sampleLinear(pixelCoord + pixelSize * vec2(0, -1));
    vec3 B3 = sampleLinear(pixelCoord + pixelSize * vec2(1, -1));
    vec3 C1 = sampleLinear(pixelCoord + pixelSize * vec2(-1, 0));
    vec3 C2 = sampleLinear(pixelCoord + pixelSize * vec2(0, 0));
    vec3 C3 = sampleLinear(pixelCoord + pixelSize * vec2(1, 0));
    vec3 D1 = sampleLinear(pixelCoord + pixelSize * vec2(-1, 1));
    vec3 D2 = sampleLinear(pixelCoord + pixelSize * vec2(0, 1));
    vec3 D3 = sampleLinear(pixelCoord + pixelSize * vec2(1, 1));

    // Blend instructions for each corner of the scaled pixel
    //   TT
    //  +--+
    // L|xy|R
    // L|zw|R
    //  +--+
    //   BB
    ivec4 blendTypes = ivec4(BLEND_NONE);

    // Top-left:
    if (!((B1 == B2 && C1 == C2) || (B1 == C1 && B2 == C2)) && C2 != B2 && C2 != C1) {
        vec3 A1 = sampleLinear(pixelCoord + pixelSize * vec2(-1, -2));
        vec3 A2 = sampleLinear(pixelCoord + pixelSize * vec2(0, -2));
        vec3 B0 = sampleLinear(pixelCoord + pixelSize * vec2(-2, -1));
        vec3 C0 = sampleLinear(pixelCoord + pixelSize * vec2(-2, 0));
        // Upward gradient = from bottom left to top right
        // Downward gradient = from top left to bottom right
        //
        // Strong upward gradient + weak downward gradient = likely diagonal line going downward toward right
        // Strong downward gradient + weak upward gradient = likely diagonal line going upward toward right
        //
        // Stronger weight is given to the imaginary line going straight through the middle of this 2x2 corner
        //
        // Weaker gradients win.
        float upwardGradient = (4 * colorDistance(B1, C2)) + colorDistance(B2, C3) + colorDistance(C1, D2) + colorDistance(A1, B2) + colorDistance(B0, C1);
        float downwardGradient = (4 * colorDistance(C1, B2)) + colorDistance(C2, B3) + colorDistance(D1, C2) + colorDistance(B1, A2) + colorDistance(C0, B1);

        bool downwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * downwardGradient) < upwardGradient;

        if (downwardGradient < upwardGradient) {
            blendTypes.x = downwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        }
    }

    // Top-right:
    if (!((B2 == B3 && C2 == C3) || (B2 == C2 && B3 == C3)) && C2 != B2 && C2 != C3) {
        vec3 A2 = sampleLinear(pixelCoord + pixelSize * vec2(0, -2));
        vec3 A3 = sampleLinear(pixelCoord + pixelSize * vec2(1, -2));
        vec3 B4 = sampleLinear(pixelCoord + pixelSize * vec2(2, -1));
        vec3 C4 = sampleLinear(pixelCoord + pixelSize * vec2(2, 0));
        // Upward gradient = from bottom left to top right
        // Downward gradient = from top left to bottom right
        //
        // Strong upward gradient + weak downward gradient = likely diagonal line going downward toward right
        // Strong downward gradient + weak upward gradient = likely diagonal line going upward toward right
        //
        // Stronger weight is given to the imaginary line going straight through the middle of this 2x2 corner
        //
        // Weaker gradients win.
        float downwardGradient = (4 * colorDistance(B2, C3)) + colorDistance(B3, C4) + colorDistance(C2, D3) + colorDistance(A2, B3) + colorDistance(B1, C2);
        float upwardGradient = (4 * colorDistance(C2, B3)) + colorDistance(C3, B4) + colorDistance(D2, C3) + colorDistance(B2, A3) + colorDistance(C1, B2);

        bool downwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * downwardGradient) < upwardGradient;

        if (upwardGradient > downwardGradient) {
            blendTypes.y = downwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        }
    }

    // Bottom-left:
    if (!((C1 == C2 && D1 == D2) || (C1 == D1 && C2 == D2)) && C2 != C1 && C2 != D2) {
        vec3 E1 = sampleLinear(pixelCoord + pixelSize * vec2(-1, 2));
        vec3 E2 = sampleLinear(pixelCoord + pixelSize * vec2(0, 2));
        vec3 C0 = sampleLinear(pixelCoord + pixelSize * vec2(-2, 0));
        vec3 D0 = sampleLinear(pixelCoord + pixelSize * vec2(-2, 1));
        // Upward gradient = from bottom right to top left
        // Downward gradient = from top right to bottom left
        //
        // Strong upward gradient + weak downward gradient = likely diagonal line going downward toward left
        // Strong downward gradient + weak upward gradient = likely diagonal line going upward toward left
        //
        // Stronger weight is given to the imaginary line going straight through the middle of this 2x2 corner
        //
        // Weaker gradients win.
        float downwardGradient = (4 * colorDistance(D2, C1)) + colorDistance(C2, D3) + colorDistance(D1, E2) + colorDistance(B1, C2) + colorDistance(C0, D1);
        float upwardGradient = (4 * colorDistance(D1, C2)) + colorDistance(D2, C3) + colorDistance(E1, D2) + colorDistance(C1, B2) + colorDistance(D0, C1);

        bool downwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * downwardGradient) < upwardGradient;

        if (upwardGradient > downwardGradient) {
            blendTypes.z = downwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        }
    }

    // Bottom-right:
    if (!((C2 == C3 && D2 == D3) || (C2 == D2 && C3 == D3)) && C2 != C3 && C2 != D2) {
        vec3 E2 = sampleLinear(pixelCoord + pixelSize * vec2(0, 2));
        vec3 E3 = sampleLinear(pixelCoord + pixelSize * vec2(1, 2));
        vec3 C4 = sampleLinear(pixelCoord + pixelSize * vec2(2, 0));
        vec3 D4 = sampleLinear(pixelCoord + pixelSize * vec2(2, 1));
        // Upward gradient = from bottom left to top right
        // Downward gradient = from top left to bottom right
        //
        // Strong upward gradient + weak downward gradient = likely diagonal line going downward toward right
        // Strong downward gradient + weak upward gradient = likely diagonal line going upward toward right
        //
        // Stronger weight is given to the imaginary line going straight through the middle of this 2x2 corner
        //
        // Weaker gradients win.
        float downwardGradient = (4 * colorDistance(C2, D3)) + colorDistance(C3, D4) + colorDistance(D2, E3) + colorDistance(B2, C3) + colorDistance(C1, D2);
        float upwardGradient = (4 * colorDistance(D2, C3)) + colorDistance(D3, C4) + colorDistance(E2, D3) + colorDistance(C2, B3) + colorDistance(D1, C2);

        bool upwardwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * upwardGradient) < downwardGradient;

        if (upwardGradient < downwardGradient) {
            blendTypes.w = upwardwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        }
    }

    vec3 color = C2;

    // Top-left blend
    if (blendTypes.x != BLEND_NONE) {
        float shallow = colorDistance(C1, B3);
        float steep = colorDistance(B2, D1);
        bool dominantCorner = blendTypes.x == BLEND_DOMINANT;
        bool bottomLeftBlend = (blendTypes.z != BLEND_NONE && !isColorSimilar(C2, B3));
        bool topRightBlend = (blendTypes.y != BLEND_NONE && !isColorSimilar(C2, D1));
        bool isCorner = !isColorSimilar(C2, B1)
                      && isColorSimilar(D1, C1)
                      && isColorSimilar(C1, B1)
                      && isColorSimilar(B1, B2)
                      && isColorSimilar(B2, B3);
        // determines whether to fill the region in by splitting it diagonally
        // and filling each side of the split with different colors, or if we should
        // fall back on nearest neighbour
        bool splitDiagonally = dominantCorner || (!bottomLeftBlend && !topRightBlend && !isCorner);
        vec2 origin = vec2(0, -1 / sqrt(2));
        vec2 direction = vec2(-1, 1);
        if (splitDiagonally) {
            float isShallow = ((STEEPNESS_THRESHOLD * shallow <= steep) && C2 != B3 && C3 != B3) ? 1 : 0;
            float isSteep = ((STEEPNESS_THRESHOLD * steep <= shallow) && C2 != D1 && D1 != D2) ? 1 : 0;
            origin = vec2(0, -0.5 * (1 - 0.5 * isShallow));
            direction.x -= isShallow;
            direction.y += isSteep;
        }
        vec3 fillColor = mix(C1, B2, step(colorDistance(C2, B2), colorDistance(C2, C1)));
        color = mix(color, fillColor, scaledFillRatio(pixelOffset, origin, direction, scale));
    }

    // Top-right blend
    if (blendTypes.y != BLEND_NONE) {
        float shallow = colorDistance(C3, B1);
        float steep = colorDistance(B2, D3);
        bool dominantCorner = blendTypes.y == BLEND_DOMINANT;
        bool bottomRightBlend = (blendTypes.w != BLEND_NONE && !isColorSimilar(C2, B1));
        bool topLeftBlend = (blendTypes.x != BLEND_NONE && !isColorSimilar(C2, D3));
        bool isCorner = !isColorSimilar(C2, B3)
                      && isColorSimilar(D3, C3)
                      && isColorSimilar(C3, B3)
                      && isColorSimilar(B3, B2)
                      && isColorSimilar(B2, B1);
        // determines whether to fill the region in by splitting it diagonally
        // and filling each side of the split with different colors, or if we should
        // fall back on nearest neighbour
        bool splitDiagonally = dominantCorner || (!bottomRightBlend && !topLeftBlend && !isCorner);
        vec2 origin = vec2(1 / sqrt(2), 0);
        vec2 direction = vec2(-1, -1);
        if (splitDiagonally) {
            float isShallow = ((STEEPNESS_THRESHOLD * steep <= shallow) && C2 != D3 && D2 != D3) ? 1 : 0;
            float isSteep = ((STEEPNESS_THRESHOLD * shallow <= steep) && C2 != B1 && C1 != B1) ? 1 : 0;
            origin = vec2(0.5 * (1 - 0.5 * isShallow), 0);
            direction.x -= isSteep;
            direction.y -= isShallow;
        }
        vec3 fillColor = mix(C3, B2, step(colorDistance(C2, B2), colorDistance(C2, C3)));
        color = mix(color, fillColor, scaledFillRatio(pixelOffset, origin, direction, scale));
    }

    // Bottom-left blend
    if (blendTypes.z != BLEND_NONE) {
        float shallow = colorDistance(C1, D3);
        float steep = colorDistance(D2, B1);
        bool dominantCorner = blendTypes.z == BLEND_DOMINANT;
        bool topLeftBlend = (blendTypes.x != BLEND_NONE && !isColorSimilar(C2, D3));
        bool bottomRightBlend = (blendTypes.w != BLEND_NONE && !isColorSimilar(C2, B1));
        bool isCorner = !isColorSimilar(C2, D1)
                     && isColorSimilar(D3, D2)
                     && isColorSimilar(D2, D1)
                     && isColorSimilar(D1, C1)
                     && isColorSimilar(C1, B1);
        // determines whether to fill the region in by splitting it diagonally
        // and filling each side of the split with different colors, or if we should
        // fall back on nearest neighbour
        bool splitDiagonally = dominantCorner || (!topLeftBlend && !bottomRightBlend && !isCorner);
        vec2 origin = vec2(-1 / sqrt(2), 0);
        vec2 direction = vec2(1, 1);
        if (splitDiagonally) {
            float isShallow = ((STEEPNESS_THRESHOLD * steep <= shallow) && C2 != B1 && B2 != B1) ? 1 : 0;
            float isSteep = ((STEEPNESS_THRESHOLD * shallow <= steep) && C2 != D3 && C3 != D3) ? 1 : 0;
            origin = vec2(-0.5 * (1 - 0.5 * isShallow), 0);
            direction.x += isSteep;
            direction.y += isShallow;
        }
        vec3 fillColor = mix(D2, C1, step(colorDistance(C2, C1), colorDistance(C2, D2)));
        color = mix(color, fillColor, scaledFillRatio(pixelOffset, origin, direction, scale));
    }

    // Bottom-right blend
    if (blendTypes.w != BLEND_NONE) {
        float shallow = colorDistance(C3, D1);
        float steep = colorDistance(D2, B3);
        bool dominantCorner = blendTypes.w == BLEND_DOMINANT;
        bool topRightBlend = (blendTypes.y != BLEND_NONE && !isColorSimilar(C2, D1));
        bool bottomLeftBlend = (blendTypes.z != BLEND_NONE && !isColorSimilar(C2, B3));
        bool isCorner = !isColorSimilar(C2, D3)
                      && isColorSimilar(B3, C3)
                      && isColorSimilar(C3, D3)
                      && isColorSimilar(D3, D2)
                      && isColorSimilar(D2, D1);
        // determines whether to fill the region in by splitting it diagonally
        // and filling each side of the split with different colors, or if we should
        // fall back on nearest neighbour
        bool splitDiagonally = dominantCorner || (!topRightBlend && !bottomLeftBlend && !isCorner);
        vec2 origin = vec2(0, 1 / sqrt(2));
        vec2 direction = vec2(1, -1);
        if (splitDiagonally) {
            float isShallow = ((STEEPNESS_THRESHOLD * shallow <= steep) && C2 != D1 && C1 != D1) ? 1 : 0;
            float isSteep = ((STEEPNESS_THRESHOLD * steep <= shallow) && C2 != B3 && B3 != B2) ? 1 : 0;
            origin = vec2(0, 0.5 * (1 - 0.5 * isShallow));
            direction.x += isShallow;
            direction.y -= isSteep;
        }
        vec3 fillColor = mix(D2, C3, step(colorDistance(C2, C3), colorDistance(C2, D2)));
        color = mix(color, fillColor, scaledFillRatio(pixelOffset, origin, direction, scale));
    }

    fragColor = nitsToDestinationEncoding(vec4(color, 1.0));
}

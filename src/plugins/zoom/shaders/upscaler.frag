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

#define BLEND_NONE 0
#define BLEND_NORMAL 1
#define BLEND_DOMINANT 2
#define STEEPNESS_THRESHOLD 2.2

in vec2 texcoord0;

out vec4 fragColor;

#define INTENSITY_WEIGHT 1.0
#define COLOR_SIMULARITY_TOLERANCE 0.1127
#define DOMINANT_DIRECTION_THRESHOLD 3.6

/* compute distance between two colors, in ICtCp space, with more weight given to intensity */
float colorDistance(vec3 a, vec3 b) {
    vec3 lmsA = (destinationToLMS * vec4(a / sourceReferenceLuminance, 1.0)).rgb;
    vec3 lmsB = (destinationToLMS * vec4(b / sourceReferenceLuminance, 1.0)).rgb;
    vec3 lmsA_PQ = linearToPq(lmsA);
    vec3 lmsB_PQ = linearToPq(lmsB);
    vec3 ICtCpA = toICtCp * lmsA_PQ;
    vec3 ICtCpB = toICtCp * lmsB_PQ;

    vec3 diff = ICtCpA - ICtCpB;

    float i = (diff.x) * INTENSITY_WEIGHT;
    float ct = diff.y;
    float cp = diff.z;

    return sqrt((i * i) + (ct * ct) + (cp * cp));
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

vec3 blendTypeToDebugColor(int blend) {
    if (blend == BLEND_DOMINANT)
    return vec3(1, 0, 0);
    if (blend == BLEND_NORMAL)
    return vec3(0,0, 1);
    return vec3(0);
}

void main()
{
    vec2 textureSize = vec2(textureWidth, textureHeight);
    vec2 pixelSize = 1 / textureSize;
    vec2 scale = vec2(zoomLevel);
    vec2 pixelOffset = (fract(texcoord0 * textureSize) - 0.5);
    vec2 pixelCoord = texcoord0 - pixelOffset * pixelSize;
    vec2 texelOffset = pixelOffset * pixelSize;

    #define PIXEL(x,y) texture(sampler, pixelCoord + pixelSize * vec2(x, y)).rgb

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
    vec3 B1 = PIXEL(-1, -1);
    vec3 B2 = PIXEL(0, -1);
    vec3 B3 = PIXEL(1, -1);
    vec3 C1 = PIXEL(-1, 0);
    vec3 C2 = PIXEL(0, 0);
    vec3 C3 = PIXEL(1, 0);
    vec3 D1 = PIXEL(-1, 1);
    vec3 D2 = PIXEL(0, 1);
    vec3 D3 = PIXEL(1, 1);

    // Blend instructions for each corner of the scaled pixel
    //   TT
    //  +--+
    // L|zy|R
    // L|zw|R
    //  +--+
    //   BB
    ivec4 blendTypes = ivec4(0);

    // Top-left:
    if (!((B1 == B2) && (C1 == C2) || (B1 == C1 && B2 == C2))) {
        vec3 A1 = PIXEL(-1, -2);
        vec3 A2 = PIXEL(0, -2);
        vec3 B0 = PIXEL(-2, -1);
        vec3 C0 = PIXEL(-2, 0);
        float upward = (4 * colorDistance(B1, C2)) + colorDistance(B2, C3) + colorDistance(C1, D2) + colorDistance(A1, B2) + colorDistance(B0, C1);
        float downward = (4 * colorDistance(C1, B2)) + colorDistance(C2, B3) + colorDistance(D1, C2) + colorDistance(B1, A2) + colorDistance(C0, B1);

        bool downwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * downward) < upward;

        if (C2 != B2 && C2 != C1 && downward < upward) {
            blendTypes.x = downwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        } else {
            blendTypes.x = BLEND_NONE;
        }
    }

    // Top-right:
    if (!((B2 == B3) && (C2 == C3) || (B2 == C2 && B3 == C3))) {
        vec3 A2 = PIXEL(0, -2);
        vec3 A3 = PIXEL(1, -2);
        vec3 B4 = PIXEL(2, -1);
        vec3 C4 = PIXEL(2, 0);
        float upward = (4 * colorDistance(B2, C3)) + colorDistance(B3, C4) + colorDistance(C2, D3) + colorDistance(A2, B3) + colorDistance(B1, C2);
        float downward = (4 * colorDistance(C2, B3)) + colorDistance(C3, B4) + colorDistance(D2, C3) + colorDistance(B2, A3) + colorDistance(C1, B2);

        bool upwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * upward) < downward;

        if (C2 != B2 && C2 != C3 && downward > upward) {
            blendTypes.y = upwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        } else {
            blendTypes.y = BLEND_NONE;
        }
    }

    // Bottom-left:
    if (!((C1 == C2) && (D1 == D2) || (C1 == D1 && C2 == D2))) {
        vec3 E1 = PIXEL(-1, 2);
        vec3 E2 = PIXEL(0, 2);
        vec3 C0 = PIXEL(-2, 0);
        vec3 D0 = PIXEL(-2, 1);
        float downward = (4 * colorDistance(D2, C1)) + colorDistance(C2, D3) + colorDistance(D1, E2) + colorDistance(B1, C2) + colorDistance(C0, D1);
        float upward = (4 * colorDistance(D1, C2)) + colorDistance(D2, C3) + colorDistance(E1, D2) + colorDistance(C1, B2) + colorDistance(D0, C1);

        bool downwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * downward) < upward;

        if (C2 != C1 && C2 != D2 && upward > downward) {
            blendTypes.z = downwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        } else {
            blendTypes.z = BLEND_NONE;
        }
    }

    // Bottom-right:
    if (!((C2 == C3) && (D2 == D3) || (C2 == D2 && C3 == D3))) {
        vec3 E2 = PIXEL(0, 2);
        vec3 E3 = PIXEL(1, 2);
        vec3 C4 = PIXEL(2, 0);
        vec3 D4 = PIXEL(2, 1);
        float downward = (4 * colorDistance(C2, D3)) + colorDistance(C3, D4) + colorDistance(D2, E3) + colorDistance(B2, C3) + colorDistance(C1, D2);
        float upward = (4 * colorDistance(D2, C3)) + colorDistance(D3, C4) + colorDistance(E2, D3) + colorDistance(C2, B3) + colorDistance(D1, C2);

        bool upwardwardIsDominant = (DOMINANT_DIRECTION_THRESHOLD * upward) < downward;

        if (C2 != C3 && C2 != D2 && upward < downward) {
            blendTypes.w = upwardwardIsDominant ? BLEND_DOMINANT : BLEND_NORMAL;
        } else {
            blendTypes.w = BLEND_NONE;
        }
    }

    vec3 color = C2;

    // Top-left blend
    if (blendTypes.x != BLEND_NONE) {
        float shallow = colorDistance(C1, B3);
        float steep = colorDistance(B2, D1);
        bool splitByImage = blendTypes.x == BLEND_DOMINANT || !(
        (blendTypes.z != BLEND_NONE && !isColorSimilar(C2, B3))
        || (blendTypes.y != BLEND_NONE && !isColorSimilar(C2, D1))
        || (
        !isColorSimilar(C2, B1)
        && isColorSimilar(D1, C1)
        && isColorSimilar(C1, B1)
        && isColorSimilar(B1, B2)
        && isColorSimilar(B2, B3)
        )
        );

        vec2 origin = vec2(0, -1 / sqrt(2));
        vec2 direction = vec2(-1, 1);
        if (splitByImage) {
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
        bool splitByImage = blendTypes.y == BLEND_DOMINANT || !(
        (blendTypes.w != BLEND_NONE && !isColorSimilar(C2, B1))
        || (blendTypes.x != BLEND_NONE && !isColorSimilar(C2, D3))
        || (
        !isColorSimilar(C2, B3)
        && isColorSimilar(D3, C3)
        && isColorSimilar(C3, B3)
        && isColorSimilar(B3, B2)
        && isColorSimilar(B2, B1)
        )
        );

        vec2 origin = vec2(1 / sqrt(2), 0);
        vec2 direction = vec2(-1, -1);
        if (splitByImage) {
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
        bool splitByImage = blendTypes.z == BLEND_DOMINANT || !(
        (blendTypes.x != BLEND_NONE && !isColorSimilar(C2, D3))
        || (blendTypes.w != BLEND_NONE && !isColorSimilar(C2, B1))
        || (
        !isColorSimilar(C2, D1)
        && isColorSimilar(D3, D2)
        && isColorSimilar(D2, D1)
        && isColorSimilar(D1, C1)
        && isColorSimilar(C1, B1)
        )
        );

        vec2 origin = vec2(-1 / sqrt(2), 0);
        vec2 direction = vec2(1, 1);
        if (splitByImage) {
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
        bool splitByImage = blendTypes.w == BLEND_DOMINANT || !(
        (blendTypes.y != BLEND_NONE && !isColorSimilar(C2, D1))
        || (blendTypes.z != BLEND_NONE && !isColorSimilar(C2, B3))
        || (
        !isColorSimilar(C2, D3)
        && isColorSimilar(B3, C3)
        && isColorSimilar(C3, D3)
        && isColorSimilar(D3, D2)
        && isColorSimilar(D2, D1)
        )
        );

        vec2 origin = vec2(0, 1 / sqrt(2));
        vec2 direction = vec2(1, -1);
        if (splitByImage) {
            float isShallow = ((STEEPNESS_THRESHOLD * shallow <= steep) && C2 != D1 && C1 != D1) ? 1 : 0;
            float isSteep = ((STEEPNESS_THRESHOLD * steep <= shallow) && C2 != B3 && B3 != B2) ? 1 : 0;
            origin = vec2(0, 0.5 * (1 - 0.5 * isShallow));
            direction.x += isShallow;
            direction.y -= isSteep;
        }
        vec3 fillColor = mix(D2, C3, step(colorDistance(C2, C3), colorDistance(C2, D2)));
        color = mix(color, fillColor, scaledFillRatio(pixelOffset, origin, direction, scale));
    }

    vec4 nits = sourceEncodingToNitsInDestinationColorspace(vec4(color, 1));
    fragColor = nitsToDestinationEncoding(nits);
}

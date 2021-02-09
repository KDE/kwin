/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008, 2011 Martin Gräßlin <kde@martin-graesslin.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#version 140
uniform mat4 modelViewProjectionMatrix;
uniform float width;
uniform float height;
uniform float cubeAngle;
uniform vec2 u_offset;
uniform float timeLine;

in vec4 position;
in vec4 texcoord;

out vec2 texcoord0;

void main()
{
    texcoord0 = texcoord.st;
    vec4 transformedVertex = position;
    transformedVertex.x = transformedVertex.x - width;
    transformedVertex.y = transformedVertex.y - height;
    transformedVertex.xy = transformedVertex.xy + u_offset;
    float radian = radians(cubeAngle);
    float radius = (width)/cos(radian);
    float zenithAngle = acos(transformedVertex.y/radius);
    float azimuthAngle = asin(transformedVertex.x/radius);
    transformedVertex.z = radius * sin( zenithAngle ) * cos( azimuthAngle ) - radius*cos( radians( 90.0 - cubeAngle ) );
    transformedVertex.x = radius * sin( zenithAngle ) * sin( azimuthAngle );

    transformedVertex.xy += vec2( width - u_offset.x, height - u_offset.y );

    vec3 diff = (position.xyz - transformedVertex.xyz)*timeLine;
    transformedVertex.xyz += diff;

    gl_Position = modelViewProjectionMatrix*transformedVertex;
}

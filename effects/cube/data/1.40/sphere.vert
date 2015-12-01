/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2011 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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

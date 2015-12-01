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
uniform mat4 modelViewProjectionMatrix;
uniform float width;
uniform float cubeAngle;
uniform float xCoord;
uniform float timeLine;

attribute vec4 position;
attribute vec4 texcoord;

varying vec2 texcoord0;

void main()
{
    texcoord0 = texcoord.st;
    vec4 transformedVertex = vec4(position.x - ( width - xCoord ), position.yzw);
    float radian = radians(cubeAngle);
    float radius = (width)*tan(radian);
    float azimuthAngle = radians(transformedVertex.x/(width)*(90.0 - cubeAngle));

    transformedVertex.x = width - xCoord + radius * sin( azimuthAngle );
    transformedVertex.z = position.z + radius * cos( azimuthAngle ) - radius;

    vec3 diff = (position.xyz - transformedVertex.xyz)*timeLine;
    transformedVertex.xyz += diff;

    gl_Position = modelViewProjectionMatrix*transformedVertex;
}

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
uniform mat4 projection;
uniform mat4 modelview;
uniform mat4 screenTransformation;
uniform mat4 windowTransformation;
uniform float width;
uniform float cubeAngle;
uniform float xCoord;
uniform float timeLine;

attribute vec4 vertex;
attribute vec2 texCoord;

varying vec2 varyingTexCoords;

void main()
{
    varyingTexCoords = texCoord;
    vec4 transformedVertex = vec4(vertex.x - ( width - xCoord ), vertex.yzw);
    float radian = radians(cubeAngle);
    float radius = (width)*tan(radian);
    float azimuthAngle = radians(transformedVertex.x/(width)*(90.0 - cubeAngle));

    transformedVertex.x = width - xCoord + radius * sin( azimuthAngle );
    transformedVertex.z = vertex.z + radius * cos( azimuthAngle ) - radius;

    vec3 diff = (vertex.xyz - transformedVertex.xyz)*timeLine;
    transformedVertex.xyz += diff;

    gl_Position = projection*(modelview*screenTransformation*windowTransformation)*transformedVertex;
}

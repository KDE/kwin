/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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
uniform float width;
uniform float cubeAngle;
uniform float xCoord;
uniform float timeLine;

void main()
{
    gl_TexCoord[0].xy = gl_Vertex.xy;
    vec4 vertex = vec4(gl_Vertex.x - ( width*0.5 - xCoord ), gl_Vertex.yzw);
    float radian = radians(cubeAngle*0.5);
    float radius = (width*0.5)*tan(radian);
    float azimuthAngle = radians(vertex.x/(width*0.5)*(90.0 - cubeAngle*0.5));

    vertex.x = width*0.5 - xCoord + radius * sin( azimuthAngle );
    vertex.z = gl_Vertex.z + radius * cos( azimuthAngle ) - radius;

    vec3 diff = (gl_Vertex.xyz - vertex.xyz)*timeLine;
    vertex.xyz += diff;
    gl_Position = gl_ModelViewProjectionMatrix * vertex;
    gl_FrontColor = gl_Color;
}

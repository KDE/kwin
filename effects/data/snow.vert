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
attribute float angle;
attribute float x;
attribute float y;

void main()
{
    float radian = radians(angle);
    mat2 rotation = mat2( cos(radian), sin(radian), -sin(radian), cos(radian) );
    vec2 vertex = (gl_Vertex.xy-vec2(x, y))*rotation+vec2(x, y);
    gl_TexCoord[0]  = gl_MultiTexCoord0;
    gl_Position = gl_ModelViewProjectionMatrix * vec4(vertex, gl_Vertex.zw);
}

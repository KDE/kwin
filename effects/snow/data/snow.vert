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
uniform float angle;
uniform int x;
uniform int width;
uniform int height;
varying float discarding;
uniform int hSpeed;
uniform int vSpeed;
uniform int frames;

void main()
{
    gl_TexCoord[0]  = gl_MultiTexCoord0;
    float radian = radians( angle*float(frames) );
    mat2 rotation = mat2( cos(radian), sin(radian), -sin(radian), cos(radian) );
    float xCoord;
    float yCoord;
    if( gl_MultiTexCoord0.s == 0.0 )
        xCoord =  -float(width)*0.5;
    else
        xCoord = float(width)*0.5;
    if( gl_MultiTexCoord0.t == 0.0 )
        yCoord =  -float(height)*0.5;
    else
        yCoord = float(height)*0.5;
    vec2 vertex = vec2( xCoord, yCoord );
    vertex = (vertex)*rotation + vec2( float(x), float(-height) ) + vec2(hSpeed, vSpeed)*float(frames);
    gl_Position = gl_ModelViewProjectionMatrix * vec4(vertex, gl_Vertex.zw);
}

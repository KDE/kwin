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
uniform float height;
uniform float cubeAngle;
uniform float xCoord;
uniform float yCoord;

void main()
{
    gl_TexCoord[0].xy = gl_Vertex.xy;
    vec3 vertex = vec3( gl_Vertex.xy - vec2( width*0.5 - xCoord, height*0.5 - yCoord ), gl_Vertex.z );
    float radian = radians(cubeAngle*0.5);
    float radius = (width*0.5)/cos(radian);
    float zenithAngle = acos( vertex.y/radius );
    float azimuthAngle = asin( vertex.x/radius );
    vertex.z = radius * sin( zenithAngle ) * cos( azimuthAngle ) - radius*cos( radians( 90.0 - cubeAngle*0.5 ) );
    vertex.x = radius * sin( zenithAngle ) * sin( azimuthAngle );

    vertex.xy += vec2( width*0.5 - xCoord, height*0.5 - yCoord );

    gl_Position = gl_ModelViewProjectionMatrix * vec4( vertex, 1.0 );
}

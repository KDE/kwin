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
uniform float origWidth;

void main()
{
    gl_TexCoord[0].xy = gl_Vertex.xy;
    vec4 vertex = gl_Vertex.xyzw;
    float radian = radians(cubeAngle*0.5);
    // height of the triangle compound of  one side of the cube and the two bisecting lines
    float midpoint = origWidth*0.5*tan(radian);
    // radius of the circle
    float radius = (origWidth*0.5)/cos(radian);

    // the calculation does the following:
    // At every x position we move on to the circle und create a new circular segment
    // The distance between the new chord and the desktop (z=0) is the wanted z value
    // The length of the new chord is 2*distance(x,middle of desktop(width*0.5))
    // Now we calculate the angle between chord and radius as acos(distance/radius)
    // With this angle we can can calculate the height of the new triangle (radius-distance*2-radius)
    // The height is the opposite leg of the triangle compound of distance-*height*-radius
    // New height minus old height (midpoint) is the looked for z-value
    
    // distance from midpoint of desktop to x coord
    float distance = width*0.5 - (vertex.x+xCoord);
    if( (vertex.x+xCoord) > width*0.5 )
        {
        distance = (vertex.x+xCoord) - width*0.5;
        }
    // distance in correct format
    distance = distance/(width*0.5)*origWidth*0.5;
    float angle = acos( distance/radius );
    float h = radius;
    // if distance == 0 -> angle=90 -> tan(90) singularity
    if( distance != 0.0 )
        h = tan( angle ) * distance;
    vertex.z = h - midpoint;
    gl_Position = gl_ModelViewProjectionMatrix * vertex;
}

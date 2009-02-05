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
uniform sampler2D snowTexture;
uniform int left;
uniform int right;
uniform int top;
uniform int bottom;

void main()
{
    gl_FragColor = texture2D( snowTexture, gl_TexCoord[0].st );
    // manual clipping
    if( gl_FragCoord.x < float( left ) || gl_FragCoord.x > float( right )
        || gl_FragCoord.y < float( top ) || gl_FragCoord.y > float( bottom ) )
        discard;
}

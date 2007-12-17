/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

/*

 The only purpose of this file is to be later in the link order than
 (nvidia's) libGL, thus being initialized by the dynamic linker before it,
 allowing it to set __GL_YIELD=NOTHING soon enough for libGL to notice it.

*/

#include <stdlib.h>
#include <kdefakes.h>

class kwinnvidiahack
    {
    public:
        kwinnvidiahack() { setenv( "__GL_YIELD", "NOTHING", true ); }
    };

kwinnvidiahack kwinnvidiahackinst;

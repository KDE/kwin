/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_COMPOSITINGPREFS_H
#define KWIN_COMPOSITINGPREFS_H

#include <QString>
#include <QStringList>

#include "kwinglutils.h"
#include "kwinglobals.h"


namespace KWin
{

class CompositingPrefs
{
public:
    CompositingPrefs();
    ~CompositingPrefs();

    static bool compositingPossible();
    static QString compositingNotPossibleReason();
    static bool openGlIsBroken();
    /**
     * Tests whether GLX is supported and returns @c true
     * in case KWin is compiled with OpenGL support and GLX
     * is available.
     *
     * If KWin is compiled with OpenGL ES or without OpenGL at
     * all, @c false is returned.
     * @returns @c true if GLX is available, @c false otherwise and if not build with OpenGL support.
     **/
    static bool hasGlx();
    bool enableDirectRendering() const  {
        return mEnableDirectRendering;
    }

    void detect();

private:
    bool mEnableDirectRendering;
};

}

#endif //KWIN_COMPOSITINGPREFS_H



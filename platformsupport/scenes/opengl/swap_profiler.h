/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_SCENE_OPENGL_SWAP_PROFILER_H
#define KWIN_SCENE_OPENGL_SWAP_PROFILER_H

#include <QElapsedTimer>
#include <kwin_export.h>

namespace KWin
{

/**
 * @short Profiler to detect whether we have triple buffering
 * The strategy is to start setBlocksForRetrace(false) but assume blocking and have the system prove that assumption wrong
 */
class KWIN_EXPORT SwapProfiler
{
public:
    SwapProfiler();
    void init();
    void begin();
    /**
     * @return char being 'd' for double, 't' for triple (or more - but non-blocking) buffering and
     * 0 (NOT '0') otherwise, so you can act on "if (char result = SwapProfiler::end()) { fooBar(); }
     */
    char end();
private:
    QElapsedTimer m_timer;
    qint64  m_time;
    int m_counter;
};

}

#endif

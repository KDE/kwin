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
#include "swap_profiler.h"
#include <logging.h>

namespace KWin
{

SwapProfiler::SwapProfiler()
{
    init();
}

void SwapProfiler::init()
{
    m_time = 2 * 1000*1000; // we start with a long time mean of 2ms ...
    m_counter = 0;
}

void SwapProfiler::begin()
{
    m_timer.start();
}

char SwapProfiler::end()
{
    // .. and blend in actual values.
    // this way we prevent extremes from killing our long time mean
    m_time = (10*m_time + m_timer.nsecsElapsed())/11;
    if (++m_counter > 500) {
        const bool blocks = m_time > 1000 * 1000; // 1ms, i get ~250µs and ~7ms w/o triple buffering...
        qCDebug(KWIN_OPENGL) << "Triple buffering detection:" << QString(blocks ? QStringLiteral("NOT available") : QStringLiteral("Available")) <<
                        " - Mean block time:" << m_time/(1000.0*1000.0) << "ms";
        return blocks ? 'd' : 't';
    }
    return 0;
}

}

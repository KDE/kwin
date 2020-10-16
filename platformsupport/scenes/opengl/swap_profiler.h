/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

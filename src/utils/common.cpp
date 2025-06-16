/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 This file is for (very) small utility functions/classes.

*/

#include "utils/common.h"
#include "utils/c_ptr.h"

#if KWIN_BUILD_X11
#include "effect/xcb.h"
#endif

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_OPENGL, "kwin_scene_opengl", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_QPAINTER, "kwin_scene_qpainter", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_VIRTUALKEYBOARD, "kwin_virtualkeyboard", QtWarningMsg)
namespace KWin
{

#ifndef KCMRULES

//************************************
// StrutRect
//************************************

StrutRect::StrutRect(QRect rect, StrutArea area)
    : QRect(rect)
    , m_area(area)
{
}

StrutRect::StrutRect(int x, int y, int width, int height, StrutArea area)
    : QRect(x, y, width, height)
    , m_area(area)
{
}

StrutRect::StrutRect(const StrutRect &other)
    : QRect(other)
    , m_area(other.area())
{
}

StrutRect &StrutRect::operator=(const StrutRect &other)
{
    if (this != &other) {
        QRect::operator=(other);
        m_area = other.area();
    }
    return *this;
}

#if KWIN_BUILD_X11

static int server_grab_count = 0;

void grabXServer()
{
    if (++server_grab_count == 1) {
        xcb_grab_server(connection());
    }
}

void ungrabXServer()
{
    Q_ASSERT(server_grab_count > 0);
    if (--server_grab_count == 0) {
        xcb_ungrab_server(connection());
        xcb_flush(connection());
    }
}

#endif
#endif

} // namespace

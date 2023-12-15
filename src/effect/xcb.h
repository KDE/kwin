/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "config-kwin.h"

#if !KWIN_BUILD_X11
#error Do not include on non-X11 builds
#endif

#include <QGuiApplication>
#include <kwin_export.h>
#include <xcb/xcb.h>

namespace KWin
{

inline KWIN_EXPORT xcb_connection_t *connection()
{
    return reinterpret_cast<xcb_connection_t *>(qApp->property("x11Connection").value<void *>());
}

inline KWIN_EXPORT xcb_window_t rootWindow()
{
    return qApp->property("x11RootWindow").value<quint32>();
}

inline KWIN_EXPORT xcb_timestamp_t xTime()
{
    return qApp->property("x11Time").value<xcb_timestamp_t>();
}

void KWIN_EXPORT grabXServer();
void KWIN_EXPORT ungrabXServer();
bool KWIN_EXPORT grabXKeyboard(xcb_window_t w = XCB_WINDOW_NONE);
void KWIN_EXPORT ungrabXKeyboard();

/**
 * Small helper class which performs grabXServer in the ctor and
 * ungrabXServer in the dtor. Use this class to ensure that grab and
 * ungrab are matched.
 */
class XServerGrabber
{
public:
    XServerGrabber()
    {
        grabXServer();
    }
    ~XServerGrabber()
    {
        ungrabXServer();
    }
};

}

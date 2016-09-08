/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef TESTUTILS_H
#define TESTUTILS_H
// KWin
#include <kwinglobals.h>
// XCB
#include <xcb/xcb.h>

namespace {
    static void forceXcb() {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
    }
}

namespace KWin {

/**
 * Wrapper to create an 0,0x10,10 input only window for testing purposes
 **/
static xcb_window_t createWindow()
{
    xcb_window_t w = xcb_generate_id(connection());
    const uint32_t values[] = { true };
    xcb_create_window(connection(), 0, w, rootWindow(),
                      0, 0, 10, 10,
                      0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT,
                      XCB_CW_OVERRIDE_REDIRECT, values);
    return w;
}

/**
 * casts XCB_WINDOW_NONE to uint32_t. Needed to make QCOMPARE working.
 **/
static uint32_t noneWindow()
{
    return XCB_WINDOW_NONE;
}

} // namespace

#endif

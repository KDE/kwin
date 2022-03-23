/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TESTUTILS_H
#define TESTUTILS_H
// KWin
#include <kwinglobals.h>
// XCB
#include <xcb/xcb.h>

namespace
{
static void forceXcb()
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
}
}

namespace KWin
{

/**
 * Wrapper to create an 0,0x10,10 input only window for testing purposes
 */
#ifndef NO_NONE_WINDOW
static xcb_window_t createWindow()
{
    xcb_window_t w = xcb_generate_id(connection());
    const uint32_t values[] = {true};
    xcb_create_window(connection(), 0, w, rootWindow(),
                      0, 0, 10, 10,
                      0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT,
                      XCB_CW_OVERRIDE_REDIRECT, values);
    return w;
}
#endif

/**
 * casts XCB_WINDOW_NONE to uint32_t. Needed to make QCOMPARE working.
 */
#ifndef NO_NONE_WINDOW
static uint32_t noneWindow()
{
    return XCB_WINDOW_NONE;
}
#endif

} // namespace

#endif

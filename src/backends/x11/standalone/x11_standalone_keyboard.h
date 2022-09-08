/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "x11eventfilter.h"

#include <xkbcommon/xkbcommon.h>

#include <memory>

namespace KWin
{

class X11KeyboardFilter;

class X11Keyboard
{
public:
    X11Keyboard();
    ~X11Keyboard();

    xkb_keymap *xkbKeymap() const;
    xkb_state *xkbState() const;
    Qt::KeyboardModifiers modifiers() const;

    bool event(xcb_generic_event_t *event);

private:
    void updateKeymap();

    xkb_context *m_xkbContext = nullptr;
    xkb_keymap *m_xkbKeymap = nullptr;
    xkb_state *m_xkbState = nullptr;
    int32_t m_deviceId = 0;

    std::unique_ptr<X11KeyboardFilter> m_filter;
};

} // namespace KWin

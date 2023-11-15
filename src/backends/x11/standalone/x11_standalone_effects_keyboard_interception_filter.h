/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "x11eventfilter.h"

namespace KWin
{

class EffectsHandler;
class X11Keyboard;

class EffectsKeyboardInterceptionX11Filter : public X11EventFilter
{
public:
    explicit EffectsKeyboardInterceptionX11Filter(EffectsHandler *effects, X11Keyboard *keyboard);

    bool event(xcb_generic_event_t *event) override;

private:
    void processKey(bool press, xcb_keycode_t keycode, xcb_timestamp_t timestamp);

    EffectsHandler *m_effects;
    X11Keyboard *m_keyboard;
};

} // namespace KWin

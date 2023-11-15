/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "x11eventfilter.h"

namespace KWin
{
class EffectsHandler;

class EffectsMouseInterceptionX11Filter : public X11EventFilter
{
public:
    explicit EffectsMouseInterceptionX11Filter(xcb_window_t window, EffectsHandler *effects);

    bool event(xcb_generic_event_t *event) override;

private:
    EffectsHandler *m_effects;
    xcb_window_t m_window;
};

}

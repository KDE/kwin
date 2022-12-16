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
namespace TabBox
{

class X11Filter : public X11EventFilter
{
public:
    explicit X11Filter();

    bool event(xcb_generic_event_t *event) override;

private:
    bool buttonPress(xcb_button_press_event_t *event);
    void motion(xcb_generic_event_t *event);
    void keyPress(xcb_generic_event_t *event);
    void keyRelease(xcb_generic_event_t *event);
};

}

}

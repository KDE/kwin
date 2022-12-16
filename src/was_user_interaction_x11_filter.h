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

class WasUserInteractionX11Filter : public X11EventFilter
{
public:
    explicit WasUserInteractionX11Filter();

    bool event(xcb_generic_event_t *event) override;
};

}

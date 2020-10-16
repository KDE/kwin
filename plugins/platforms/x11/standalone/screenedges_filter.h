/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENEDGES_FILTER_H
#define KWIN_SCREENEDGES_FILTER_H
#include "x11eventfilter.h"

namespace KWin
{

class ScreenEdgesFilter : public X11EventFilter
{
public:
    explicit ScreenEdgesFilter();

    bool event(xcb_generic_event_t *event) override;
};

}

#endif

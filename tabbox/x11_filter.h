/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#ifndef KWIN_TABBOX_X11_FILTER_H
#define KWIN_TABBOX_X11_FILTER_H
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

#endif



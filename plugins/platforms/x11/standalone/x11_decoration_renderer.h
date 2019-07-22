/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_DECORATION_X11_RENDERER_H
#define KWIN_DECORATION_X11_RENDERER_H

#include "decorations/decorationrenderer.h"

#include <xcb/xcb.h>

class QTimer;

namespace KWin
{

namespace Decoration
{

class X11Renderer : public Renderer
{
    Q_OBJECT
public:
    explicit X11Renderer(DecoratedClientImpl *client);
    ~X11Renderer() override;

    void reparent(Deleted *deleted) override;

protected:
    void render() override;

private:
    QTimer *m_scheduleTimer;
    xcb_gcontext_t m_gc;
};

}
}

#endif

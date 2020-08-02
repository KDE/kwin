/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

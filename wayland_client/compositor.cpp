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
#include "compositor.h"
#include "surface.h"

#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

Compositor::Compositor(QObject *parent)
    : QObject(parent)
    , m_compositor(nullptr)
{
}

Compositor::~Compositor()
{
    release();
}

void Compositor::setup(wl_compositor *compositor)
{
    Q_ASSERT(compositor);
    Q_ASSERT(!m_compositor);
    m_compositor = compositor;
}

void Compositor::release()
{
    if (!m_compositor) {
        return;
    }
    wl_compositor_destroy(m_compositor);
    m_compositor = nullptr;
}

void Compositor::destroy()
{
    if (!m_compositor) {
        return;
    }
    free(m_compositor);
    m_compositor = nullptr;
}

Surface *Compositor::createSurface(QObject *parent)
{
    Q_ASSERT(isValid());
    Surface *s = new Surface(parent);
    s->setup(wl_compositor_create_surface(m_compositor));
    return s;
}

}
}

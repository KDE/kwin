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
#include "shell.h"
#include "output.h"
#include "surface.h"

namespace KWin
{
namespace Wayland
{

Shell::Shell(QObject *parent)
    : QObject(parent)
    , m_shell(nullptr)
{
}

Shell::~Shell()
{
    release();
}

void Shell::destroy()
{
    if (!m_shell) {
        return;
    }
    emit interfaceAboutToBeDestroyed();
    free(m_shell);
    m_shell = nullptr;
}

void Shell::release()
{
    if (!m_shell) {
        return;
    }
    emit interfaceAboutToBeReleased();
    wl_shell_destroy(m_shell);
    m_shell = nullptr;
}

void Shell::setup(wl_shell *shell)
{
    Q_ASSERT(!m_shell);
    Q_ASSERT(shell);
    m_shell = shell;
}

ShellSurface *Shell::createSurface(wl_surface *surface, QObject *parent)
{
    Q_ASSERT(isValid());
    ShellSurface *s = new ShellSurface(parent);
    connect(this, &Shell::interfaceAboutToBeReleased, s, &ShellSurface::release);
    connect(this, &Shell::interfaceAboutToBeDestroyed, s, &ShellSurface::destroy);
    s->setup(wl_shell_get_shell_surface(m_shell, surface));
    return s;
}

ShellSurface *Shell::createSurface(Surface *surface, QObject *parent)
{
    Q_ASSERT(surface);
    return createSurface(*surface, parent);
}

ShellSurface::ShellSurface(QObject *parent)
    : QObject(parent)
    , m_surface(nullptr)
    , m_size()
{
}

ShellSurface::~ShellSurface()
{
    release();
}

void ShellSurface::release()
{
    if (!isValid()) {
        return;
    }
    wl_shell_surface_destroy(m_surface);
    m_surface = nullptr;
}

void ShellSurface::destroy()
{
    if (!isValid()) {
        return;
    }
    free(m_surface);
    m_surface = nullptr;
}

const struct wl_shell_surface_listener ShellSurface::s_listener = {
    ShellSurface::pingCallback,
    ShellSurface::configureCallback,
    ShellSurface::popupDoneCallback
};

void ShellSurface::configureCallback(void *data, wl_shell_surface *shellSurface, uint32_t edges, int32_t width, int32_t height)
{
    Q_UNUSED(edges)
    ShellSurface *s = reinterpret_cast<ShellSurface*>(data);
    Q_ASSERT(s->m_surface == shellSurface);
    s->setSize(QSize(width, height));
}

void ShellSurface::pingCallback(void *data, wl_shell_surface *shellSurface, uint32_t serial)
{
    ShellSurface *s = reinterpret_cast<ShellSurface*>(data);
    Q_ASSERT(s->m_surface == shellSurface);
    s->ping(serial);
}

void ShellSurface::popupDoneCallback(void *data, wl_shell_surface *shellSurface)
{
    // not needed, we don't have popups
    Q_UNUSED(data)
    Q_UNUSED(shellSurface)
}

void ShellSurface::setup(wl_shell_surface *surface)
{
    Q_ASSERT(surface);
    Q_ASSERT(!m_surface);
    m_surface = surface;
    wl_shell_surface_add_listener(m_surface, &s_listener, this);
}

void ShellSurface::ping(uint32_t serial)
{
    wl_shell_surface_pong(m_surface, serial);
    emit pinged();
}

void ShellSurface::setSize(const QSize &size)
{
    if (m_size == size) {
        return;
    }
    m_size = size;
    emit sizeChanged(size);
}

void ShellSurface::setFullscreen(Output *output)
{
    Q_ASSERT(isValid());
    wl_shell_surface_set_fullscreen(m_surface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, output ? output->output() : nullptr);
}

}
}

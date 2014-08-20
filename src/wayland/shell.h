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
#ifndef KWIN_WAYLAND_SHELL_H
#define KWIN_WAYLAND_SHELL_H

#include <QObject>
#include <QSize>

#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{
class ShellSurface;
class Output;

class Shell : public QObject
{
    Q_OBJECT
public:
    explicit Shell(QObject *parent = nullptr);
    virtual ~Shell();

    bool isValid() const {
        return m_shell != nullptr;
    }
    void release();
    void destroy();
    void setup(wl_shell *shell);

    ShellSurface *createSurface(wl_surface *surface, QObject *parent = nullptr);

    operator wl_shell*() {
        return m_shell;
    }
    operator wl_shell*() const {
        return m_shell;
    }

Q_SIGNALS:
    void interfaceAboutToBeReleased();
    void interfaceAboutToBeDestroyed();

private:
    wl_shell *m_shell;
};

class ShellSurface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged)
public:
    explicit ShellSurface(QObject *parent);
    virtual ~ShellSurface();

    void release();
    void destroy();
    void setup(wl_shell_surface *surface);
    QSize size() const {
        return m_size;
    }
    void setSize(const QSize &size);

    void setFullscreen(Output *output = nullptr);

    bool isValid() const {
        return m_surface != nullptr;
    }
    operator wl_shell_surface*() {
        return m_surface;
    }
    operator wl_shell_surface*() const {
        return m_surface;
    }

    static void pingCallback(void *data, struct wl_shell_surface *shellSurface, uint32_t serial);
    static void configureCallback(void *data, struct wl_shell_surface *shellSurface, uint32_t edges, int32_t width, int32_t height);
    static void popupDoneCallback(void *data, struct wl_shell_surface *shellSurface);

Q_SIGNALS:
    void pinged();
    void sizeChanged(const QSize &);

private:
    void ping(uint32_t serial);
    wl_shell_surface *m_surface;
    QSize m_size;
    static const struct wl_shell_surface_listener s_listener;
};

}
}

#endif

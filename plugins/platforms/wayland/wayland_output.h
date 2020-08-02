/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_WAYLAND_OUTPUT_H
#define KWIN_WAYLAND_OUTPUT_H

#include "abstract_wayland_output.h"

#include <KWayland/Client/xdgshell.h>

#include <QObject>

namespace KWayland
{
namespace Client
{
class Surface;

class Shell;
class ShellSurface;

class Pointer;
class LockedPointer;
}
}

namespace KWin
{
namespace Wayland
{
class WaylandBackend;

class WaylandOutput : public AbstractWaylandOutput
{
    Q_OBJECT
public:
    WaylandOutput(KWayland::Client::Surface *surface, WaylandBackend *backend);
    ~WaylandOutput() override;

    void init(const QPoint &logicalPosition, const QSize &pixelSize);

    virtual void lockPointer(KWayland::Client::Pointer *pointer, bool lock) {
        Q_UNUSED(pointer)
        Q_UNUSED(lock)
    }

    virtual bool pointerIsLocked() { return false; }

    /**
     * @brief defines the geometry of the output
     * @param logicalPosition top left position of the output in compositor space
     * @param pixelSize output size as seen from the outside
     */
    void setGeometry(const QPoint &logicalPosition, const QSize &pixelSize);

    KWayland::Client::Surface* surface() const {
        return m_surface;
    }

    bool rendered() const {
        return m_rendered;
    }
    void resetRendered() {
        m_rendered = false;
    }

Q_SIGNALS:
    void sizeChanged(const QSize &size);
    void frameRendered();

protected:
    WaylandBackend *backend() {
        return m_backend;
    }

private:
    KWayland::Client::Surface *m_surface;
    WaylandBackend *m_backend;

    bool m_rendered = false;
};

class XdgShellOutput : public WaylandOutput
{
public:
    XdgShellOutput(KWayland::Client::Surface *surface,
                   KWayland::Client::XdgShell *xdgShell,
                   WaylandBackend *backend, int number);
    ~XdgShellOutput() override;

    void lockPointer(KWayland::Client::Pointer *pointer, bool lock) override;

private:
    void handleConfigure(const QSize &size, KWayland::Client::XdgShellSurface::States states, quint32 serial);
    void updateWindowTitle();

    KWayland::Client::XdgShellSurface *m_xdgShellSurface = nullptr;
    int m_number;
    KWayland::Client::LockedPointer *m_pointerLock = nullptr;
    bool m_hasPointerLock = false;
};

}
}

#endif

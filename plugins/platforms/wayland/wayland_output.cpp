/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

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

#include "wayland_output.h"
#include "wayland_backend.h"

#include "wayland_server.h"

#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/shell.h>

#include <KWayland/Server/display.h>

#include <KLocalizedString>

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;

WaylandOutput::WaylandOutput(Surface *surface, QObject *parent)
    : AbstractWaylandOutput(parent),
      m_surface(surface)
{
    connect(surface, &Surface::frameRendered, [this] {
        m_rendered = true;
        emit frameRendered();
    });
}

WaylandOutput::~WaylandOutput()
{
    m_surface->destroy();
    delete m_surface;
}

QSize WaylandOutput::pixelSize() const
{
    return m_pixelSize;
}

void WaylandOutput::setGeometry(const QPoint &logicalPosition, const QSize &pixelSize)
{
    m_pixelSize = pixelSize;
    setRawPhysicalSize(pixelSize);
    setGlobalPos(logicalPosition);
}

ShellOutput::ShellOutput(Surface *surface, Shell *shell, WaylandBackend *backend)
    : WaylandOutput(surface, backend)
{
    auto shellSurface = shell->createSurface(surface, this);
    shellSurface->setToplevel();
}

ShellOutput::~ShellOutput()
{
    m_shellSurface->destroy();
    delete m_shellSurface;
}

XdgShellOutput::XdgShellOutput(Surface *surface, XdgShell *xdgShell, WaylandBackend *backend, int number)
    : WaylandOutput(surface, backend)
    , m_backend(backend)
    , m_number(number)
{
    m_xdgShellSurface = xdgShell->createSurface(surface, this);
    updateWindowTitle();

    connect(m_xdgShellSurface, &XdgShellSurface::configureRequested, this, &XdgShellOutput::handleConfigure);
    connect(m_xdgShellSurface, &XdgShellSurface::closeRequested, qApp, &QCoreApplication::quit);

    connect(backend, &WaylandBackend::pointerLockSupportedChanged, this, &XdgShellOutput::updateWindowTitle);
    connect(backend, &WaylandBackend::pointerLockChanged, this, [this](bool locked) {
        if (locked) {
            if (!m_hasPointerLock) {
                // some other output has locked the pointer
                // this surface can stop trying to lock the pointer
                lockPointer(nullptr, false);
                // set it true for the other surface
                m_hasPointerLock = true;
            }
        } else {
            // just try unlocking
            lockPointer(nullptr, false);
        }
        updateWindowTitle();
    });
}

XdgShellOutput::~XdgShellOutput()
{
    m_xdgShellSurface->destroy();
    delete m_xdgShellSurface;
}

void XdgShellOutput::handleConfigure(const QSize &size, XdgShellSurface::States states, quint32 serial)
{
    Q_UNUSED(states);
    if (size.width() == 0 || size.height() == 0) {
        return;
    }
    setGeometry(geometry().topLeft(), size);
    m_xdgShellSurface->ackConfigure(serial);
    emit sizeChanged(size);
}

void XdgShellOutput::updateWindowTitle()
{
    QString grab;
    if (m_hasPointerLock) {
        grab = i18n("Press right control to ungrab pointer");
    } else if (m_backend->pointerConstraints()) {
        grab = i18n("Press right control key to grab pointer");
    }
    const QString title = i18nc("Title of nested KWin Wayland with Wayland socket identifier as argument",
                                "KDE Wayland Compositor #%1 (%2)", m_number, waylandServer()->display()->socketName());

    if (grab.isEmpty()) {
        m_xdgShellSurface->setTitle(title);
    } else {
        m_xdgShellSurface->setTitle(title + QStringLiteral(" — ") + grab);
    }
}

void XdgShellOutput::lockPointer(Pointer *pointer, bool lock)
{
    if (!lock) {
        const bool surfaceWasLocked = m_pointerLock && m_hasPointerLock;
        delete m_pointerLock;
        m_pointerLock = nullptr;
        m_hasPointerLock = false;
        if (surfaceWasLocked) {
            emit m_backend->pointerLockChanged(false);
        }
        return;
    }

    Q_ASSERT(!m_pointerLock);
    m_pointerLock = m_backend->pointerConstraints()->lockPointer(surface(), pointer, nullptr,
                                                                 PointerConstraints::LifeTime::OneShot,
                                                                 this);
    if (!m_pointerLock->isValid()) {
        delete m_pointerLock;
        m_pointerLock = nullptr;
        return;
    }
    connect(m_pointerLock, &LockedPointer::locked, this,
        [this] {
            m_hasPointerLock = true;
            emit m_backend->pointerLockChanged(true);
        }
    );
    connect(m_pointerLock, &LockedPointer::unlocked, this,
        [this] {
            delete m_pointerLock;
            m_pointerLock = nullptr;
            m_hasPointerLock = false;
            emit m_backend->pointerLockChanged(false);
        }
    );
}

}
}

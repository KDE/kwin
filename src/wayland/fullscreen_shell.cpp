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
#include "fullscreen_shell.h"

#include <QDebug>

namespace KWin
{
namespace Wayland
{

FullscreenShell::FullscreenShell(QObject *parent)
    : QObject(parent)
    , m_shell(nullptr)
    , m_capabilityArbitraryModes(false)
    , m_capabilityCursorPlane(false)
{
}

FullscreenShell::~FullscreenShell()
{
    release();
}

void FullscreenShell::release()
{
    if (m_shell) {
        _wl_fullscreen_shell_release(m_shell);
        m_shell = nullptr;
    }
}

void FullscreenShell::destroy()
{
    if (m_shell) {
        free(m_shell);
        m_shell = nullptr;
    }
}

_wl_fullscreen_shell_listener FullscreenShell::s_fullscreenShellListener = {
    FullscreenShell::capabilitiesAnnounce
};

void FullscreenShell::setup(_wl_fullscreen_shell *shell)
{
    Q_ASSERT(!m_shell);
    Q_ASSERT(shell);
    m_shell = shell;
    _wl_fullscreen_shell_add_listener(m_shell, &s_fullscreenShellListener, this);
}

void FullscreenShell::capabilitiesAnnounce(void *data, _wl_fullscreen_shell *shell, uint32_t capability)
{
    FullscreenShell *s = reinterpret_cast<FullscreenShell*>(data);
    Q_ASSERT(shell == s->m_shell);
    s->handleCapabilities(capability);
}

void FullscreenShell::handleCapabilities(uint32_t capability)
{
    if (capability & _WL_FULLSCREEN_SHELL_CAPABILITY_ARBITRARY_MODES) {
        m_capabilityArbitraryModes = true;
        emit capabilityArbitraryModesChanged(m_capabilityArbitraryModes);
    }
    if (capability & _WL_FULLSCREEN_SHELL_CAPABILITY_CURSOR_PLANE) {
        m_capabilityCursorPlane = true;
        emit capabilityCursorPlaneChanged(m_capabilityCursorPlane);
    }
}

void FullscreenShell::present(wl_surface *surface, wl_output *output)
{
    Q_ASSERT(m_shell);
    _wl_fullscreen_shell_present_surface(m_shell, surface, _WL_FULLSCREEN_SHELL_PRESENT_METHOD_DEFAULT, output);
}

}
}

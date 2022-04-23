/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xdgshellintegration.h"
#include "wayland/display.h"
#include "wayland/xdgshell_interface.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xdgshellwindow.h"

using namespace KWaylandServer;

namespace KWin
{

/**
 * The WaylandXdgShellIntegration class is a factory class for xdg-shell windows.
 *
 * The xdg-shell protocol defines two surface roles - xdg_toplevel and xdg_popup. On the
 * compositor side, those roles are represented by XdgToplevelWindow and XdgPopupWindow,
 * respectively.
 *
 * WaylandXdgShellIntegration monitors for new xdg_toplevel and xdg_popup objects. If it
 * detects one, it will create an XdgToplevelWindow or XdgPopupWindow based on the current
 * surface role of the underlying xdg_surface object.
 */

XdgShellIntegration::XdgShellIntegration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    XdgShellInterface *shell = new XdgShellInterface(waylandServer()->display(), this);

    connect(shell, &XdgShellInterface::toplevelCreated,
            this, &XdgShellIntegration::registerXdgToplevel);
    connect(shell, &XdgShellInterface::popupCreated,
            this, &XdgShellIntegration::registerXdgPopup);
}

void XdgShellIntegration::registerXdgToplevel(XdgToplevelInterface *toplevel)
{
    // Note that the window is going to be destroyed and immediately re-created when the
    // underlying surface is unmapped. XdgToplevelWindow is re-created right away since
    // we don't want too loose any window requests that are allowed to be sent prior to
    // the first initial commit, e.g. set_maximized or set_fullscreen.
    connect(toplevel, &XdgToplevelInterface::resetOccurred,
            this, [this, toplevel] {
                createXdgToplevelWindow(toplevel);
            });

    createXdgToplevelWindow(toplevel);
}

void XdgShellIntegration::createXdgToplevelWindow(XdgToplevelInterface *toplevel)
{
    if (!workspace()) {
        qCWarning(KWIN_CORE, "An xdg-toplevel surface has been created while the compositor "
                             "is still not fully initialized. That is a compositor bug!");
        return;
    }

    Q_EMIT windowCreated(new XdgToplevelWindow(toplevel));
}

void XdgShellIntegration::registerXdgPopup(XdgPopupInterface *popup)
{
    if (!workspace()) {
        qCWarning(KWIN_CORE, "An xdg-popup surface has been created while the compositor is "
                             "still not fully initialized. That is a compositor bug!");
        return;
    }

    Q_EMIT windowCreated(new XdgPopupWindow(popup));
}

} // namespace KWin

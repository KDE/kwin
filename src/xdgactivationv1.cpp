/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xdgactivationv1.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "utils/common.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/plasmawindowmanagement.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland/xdgactivation_v1.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KDesktopFile>

namespace KWin
{

static bool isPrivilegedInWindowManagement(const ClientConnection *client)
{
    Q_ASSERT(client);
    auto requestedInterfaces = client->property("requestedInterfaces").toStringList();
    return requestedInterfaces.contains(QLatin1String("org_kde_plasma_window_management")) || requestedInterfaces.contains(QLatin1String("kde_lockscreen_overlay_v1"));
}

XdgActivationV1Integration::XdgActivationV1Integration(XdgActivationV1Interface *activation, QObject *parent)
    : QObject(parent)
{
    connect(Workspace::self(), &Workspace::windowActivated, this, [this](Window *window) {
        if (!m_activation || !window || m_lastTokenAppId != window->desktopFileName()) {
            return;
        }
        clearFeedback();
    });

    activation->setActivationTokenCreator([this](ClientConnection *client, SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId) -> QString {
        Q_ASSERT(client); // Should always be available as it's coming straight from the wayland implementation
        return requestToken(isPrivilegedInWindowManagement(client), surface, serial, seat, appId);
    });

    connect(activation, &XdgActivationV1Interface::activateRequested, this, &XdgActivationV1Integration::activateSurface);
}

QString XdgActivationV1Integration::requestToken(bool isPrivileged, SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId)
{
    auto window = waylandServer()->findWindow(surface);
    if (!isPrivileged) {
        const bool allowed = !workspace()->activeWindow()
            || workspace()->activeWindow()->surface() == surface
            || (input()->lastInputSerial() <= serial && seat->serial() >= serial);
        if (!allowed) {
            qCDebug(KWIN_CORE) << "Cannot grant a token to" << window;
            return QStringLiteral("not-granted-666");
        }
    }
    static int i = 0;
    const auto newToken = QStringLiteral("kwin-%1").arg(++i);

    bool showNotify = false;
    QIcon icon = QIcon::fromTheme(QStringLiteral("system-run"));
    if (const QString desktopFilePath = Window::findDesktopFile(appId); !desktopFilePath.isEmpty()) {
        KDesktopFile df(desktopFilePath);
        Window *window = Workspace::self()->activeWindow();
        if (!window || appId != window->desktopFileName()) {
            const auto desktop = df.desktopGroup();
            showNotify = desktop.readEntry("X-KDE-StartupNotify", desktop.readEntry("StartupNotify", true));
        }
        icon = QIcon::fromTheme(df.readIcon(), icon);
    }
    if (showNotify) {
        m_lastToken = newToken;
        m_lastTokenAppId = appId;
        m_activation = waylandServer()->plasmaActivationFeedback()->createActivation(appId);
    }
    if (isPrivileged) {
        // plasmashell doesn't have a valid serial unless it has keyboard focus,
        // this makes it so that shortcuts like Meta+1 work
        // TODO pass seat + serial to plasmashell through kglobalaccel to fix this
        // more properly
        serial = input()->lastInputSerial();
    }
    workspace()->setActivationToken(newToken, serial, appId);
    if (showNotify) {
        Q_EMIT effects->startupAdded(newToken, icon);
    }
    return newToken;
}

void XdgActivationV1Integration::activateSurface(SurfaceInterface *surface, const QString &token)
{
    Workspace *ws = Workspace::self();
    auto window = waylandServer()->findWindow(surface);
    if (!window) {
        qCWarning(KWIN_CORE) << "could not find the toplevel to activate" << surface;
        return;
    }

    if (!ws->mayActivate(window, token)) {
        window->demandAttention();
        return;
    }
    if (window->readyForPainting()) {
        ws->activateWindow(window);
    } else {
        window->setActivationToken(token);
    }
    clearFeedback();
}

void XdgActivationV1Integration::clearFeedback()
{
    if (m_activation) {
        Q_EMIT effects->startupRemoved(m_lastToken);
        m_activation.reset();
    }
}

}

#include "moc_xdgactivationv1.cpp"

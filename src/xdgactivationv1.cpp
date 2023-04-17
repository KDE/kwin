/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xdgactivationv1.h"
#include "effects.h"
#include "utils/common.h"
#include "wayland/display.h"
#include "wayland/plasmawindowmanagement_interface.h"
#include "wayland/surface_interface.h"
#include "wayland/xdgactivation_v1_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include <KApplicationTrader>
#include <KDesktopFile>

using namespace KWaylandServer;

namespace KWin
{

static bool isPrivilegedInWindowManagement(const ClientConnection *client)
{
    Q_ASSERT(client);
    auto requestedInterfaces = client->property("requestedInterfaces").toStringList();
    return requestedInterfaces.contains(QLatin1String("org_kde_plasma_window_management")) || requestedInterfaces.contains(QLatin1String("kde_lockscreen_overlay_v1"));
}

static const QString windowDesktopFileName(Window *window)
{
    QString ret = window->desktopFileName();
    if (!ret.isEmpty()) {
        return ret;
    }

    // Fallback to StartupWMClass for legacy apps
    const auto resourceName = window->resourceName();
    const auto service = KApplicationTrader::query([&resourceName](const KService::Ptr &service) {
        return service->property("StartupWMClass").toString().compare(resourceName, Qt::CaseInsensitive) == 0;
    });

    if (!service.isEmpty()) {
        ret = service.constFirst()->desktopEntryName();
    }
    return ret;
}

XdgActivationV1Integration::XdgActivationV1Integration(XdgActivationV1Interface *activation, QObject *parent)
    : QObject(parent)
{
    Workspace *ws = Workspace::self();
    connect(ws, &Workspace::windowActivated, this, [this](Window *window) {
        if (!m_currentActivationToken || !window || window->property("token").toString() == m_currentActivationToken->token) {
            return;
        }

        // We check that it's not the app that we are trying to activate
        if (windowDesktopFileName(window) != m_currentActivationToken->applicationId) {
            // But also that the new one has been requested after the token was requested
            if (window->lastUsageSerial() < m_currentActivationToken->serial) {
                return;
            }
        }

        clear();
    });
    activation->setActivationTokenCreator([this](ClientConnection *client, SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId) -> QString {
        Workspace *ws = Workspace::self();
        Q_ASSERT(client); // Should always be available as it's coming straight from the wayland implementation
        const bool isPrivileged = isPrivilegedInWindowManagement(client);
        if (!isPrivileged && ws->activeWindow() && ws->activeWindow()->surface() != surface) {
            qCWarning(KWIN_CORE) << "Cannot grant a token to" << client;
            return QStringLiteral("not-granted-666");
        }

        return requestToken(isPrivileged, surface, serial, seat, appId);
    });

    connect(activation, &XdgActivationV1Interface::activateRequested, this, &XdgActivationV1Integration::activateSurface);
}

QString XdgActivationV1Integration::requestToken(bool isPrivileged, SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId)
{
    static int i = 0;
    const auto newToken = QStringLiteral("kwin-%1").arg(++i);

    if (m_currentActivationToken) {
        clear();
    }
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
    std::unique_ptr<KWaylandServer::PlasmaWindowActivationInterface> activation;
    if (showNotify) {
        activation = waylandServer()->plasmaActivationFeedback()->createActivation(appId);
    }
    m_currentActivationToken.reset(new ActivationToken{newToken, isPrivileged, surface, serial, seat, appId, showNotify, std::move(activation)});
    if (showNotify) {
        Q_EMIT effects->startupAdded(m_currentActivationToken->token, icon);
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

    if (!m_currentActivationToken || m_currentActivationToken->token != token) {
        qCDebug(KWIN_CORE) << "Refusing to activate " << window << " (provided token: " << token << ", current token:" << (m_currentActivationToken ? m_currentActivationToken->token : QStringLiteral("null")) << ")";
        window->demandAttention();
        return;
    }

    auto ownerWindow = waylandServer()->findWindow(m_currentActivationToken->surface);
    qCDebug(KWIN_CORE) << "activating" << window << surface << "on behalf of" << m_currentActivationToken->surface << "into" << ownerWindow;
    if (!ws->activeWindow() || ws->activeWindow() == ownerWindow || ws->activeWindow()->lastUsageSerial() < m_currentActivationToken->serial || m_currentActivationToken->isPrivileged) {
        ws->activateWindow(window);
    } else {
        qCWarning(KWIN_CORE) << "Activation requested while owner isn't active" << (ownerWindow ? ownerWindow->desktopFileName() : "null")
                             << m_currentActivationToken->applicationId;
        window->demandAttention();
        clear();
    }
}

void XdgActivationV1Integration::clear()
{
    Q_ASSERT(m_currentActivationToken);
    if (m_currentActivationToken->showNotify) {
        Q_EMIT effects->startupRemoved(m_currentActivationToken->token);
    }
    m_currentActivationToken.reset();
}

}

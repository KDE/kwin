/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include <QObject>
#include <QSharedPointer>

namespace KWaylandServer
{
class SeatInterface;
class ClientConnection;
class SurfaceInterface;
class XdgActivationV1Interface;
class PlasmaWindowActivationInterface;
}

namespace KWin
{
class KWIN_EXPORT XdgActivationV1Integration : public QObject
{
    Q_OBJECT
public:
    XdgActivationV1Integration(KWaylandServer::XdgActivationV1Interface *activation, QObject *parent);

    struct ActivationToken {
        ~ActivationToken();

        const QString token;
        const KWaylandServer::ClientConnection *client;
        const KWaylandServer::SurfaceInterface *surface;
        const uint serial;
        const KWaylandServer::SeatInterface *seat;
        QString applicationId;
        QSharedPointer<KWaylandServer::PlasmaWindowActivationInterface> activation;
    };
    void activateSurface(KWaylandServer::SurfaceInterface *surface, const QString &token);

private:
    void clear();

    QScopedPointer<ActivationToken> m_currentActivationToken;
};

}

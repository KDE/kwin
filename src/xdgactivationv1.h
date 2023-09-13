/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include <QObject>
#include <QPointer>

namespace KWin
{

class SeatInterface;
class ClientConnection;
class SurfaceInterface;
class XdgActivationV1Interface;
class PlasmaWindowActivationInterface;

class KWIN_EXPORT XdgActivationV1Integration : public QObject
{
    Q_OBJECT
public:
    XdgActivationV1Integration(XdgActivationV1Interface *activation, QObject *parent);

    QString requestPrivilegedToken(SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId)
    {
        return requestToken(true, surface, serial, seat, appId);
    }
    void activateSurface(SurfaceInterface *surface, const QString &token);

private:
    QString requestToken(bool isPrivileged, SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId);
    void clear();

    struct ActivationToken
    {
        QString token;
        bool isPrivileged;
        QPointer<const SurfaceInterface> surface;
        uint serial;
        SeatInterface *seat;
        QString applicationId;
        bool showNotify;
        std::unique_ptr<PlasmaWindowActivationInterface> activation;
    };
    std::unique_ptr<ActivationToken> m_currentActivationToken;
};

}

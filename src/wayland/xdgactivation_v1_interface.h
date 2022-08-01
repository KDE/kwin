/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QVector>
#include <functional>
#include <memory>
#include <optional>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class SurfaceInterface;
class SeatInterface;
class ClientConnection;

class XdgActivationV1InterfacePrivate;

class KWIN_EXPORT XdgActivationV1Interface : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(XdgActivationV1Interface)
public:
    explicit XdgActivationV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgActivationV1Interface() override;

    using CreatorFunction = std::function<QString(ClientConnection *client, SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId)>;

    /// Provide the @p creator function that will be used to create a token given its parameters
    void setActivationTokenCreator(const CreatorFunction &creator);

Q_SIGNALS:
    /// Notifies about the @p surface being activated using @p token.
    void activateRequested(SurfaceInterface *surface, const QString &token);

private:
    friend class XdgActivationV1InterfacePrivate;
    XdgActivationV1Interface(XdgActivationV1Interface *parent);
    std::unique_ptr<XdgActivationV1InterfacePrivate> d;
};

}

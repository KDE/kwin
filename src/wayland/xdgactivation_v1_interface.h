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
class XdgForeignV2Interface;

class KWIN_EXPORT XdgActivationV1Interface : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(XdgActivationV1Interface)
public:
    explicit XdgActivationV1Interface(Display *display, XdgForeignV2Interface *xdgForeign, QObject *parent = nullptr);
    ~XdgActivationV1Interface() override;

    using CreatorFunction = std::function<QString(ClientConnection *client, SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId)>;
    /**
     * If the chosenSurface is set, this indicates to the client that it should activate the existing surface.
     * If it is not set, this indicates that the client should create a new window instead.
     * If it is not set and nextBestChoice is set instead, the client can use that to make a better choice about
     * which instance of multiple instances to open the document
     */
    struct ChooserReturn
    {
        SurfaceInterface *chosenSurface = nullptr;
        SurfaceInterface *nextBestChoice = nullptr;
    };
    using ChooserFunction = std::function<ChooserReturn(const QVector<SurfaceInterface *> &candidates, const QString &activationToken)>;

    /// Provide the @p creator function that will be used to create a token given its parameters
    void setActivationTokenCreator(const CreatorFunction &creator);
    void setWindowActivationChooser(const ChooserFunction &chooser);

Q_SIGNALS:
    /// Notifies about the @p surface being activated using @p token.
    void activateRequested(SurfaceInterface *surface, const QString &token);

private:
    friend class XdgActivationV1InterfacePrivate;
    XdgActivationV1Interface(XdgActivationV1Interface *parent);
    std::unique_ptr<XdgActivationV1InterfacePrivate> d;
};

}

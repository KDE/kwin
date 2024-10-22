/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWin
{
class ClientConnection;
class SeatInterface;
class SurfaceInterface;
class TouchInterfacePrivate;

/**
 * The TouchInterface class repserents a touchscreen associated with a wl_seat. It
 * corresponds to the Wayland interface @c wl_touch.
 */
class KWIN_EXPORT TouchInterface : public QObject
{
    Q_OBJECT

public:
    ~TouchInterface() override;

    void sendDown(SurfaceInterface *surface, qint32 id, quint32 serial, std::chrono::milliseconds time, const QPointF &localPos);
    void sendUp(ClientConnection *client, qint32 id, quint32 serial, std::chrono::milliseconds time);
    void sendCancel(SurfaceInterface *surface);
    void sendMotion(SurfaceInterface *surface, qint32 id, const QPointF &localPos, std::chrono::milliseconds time);
    void sendFrame();

private:
    explicit TouchInterface(SeatInterface *seat);
    void addToFrame(ClientConnection *client);
    std::unique_ptr<TouchInterfacePrivate> d;

    friend class SeatInterfacePrivate;
    friend class TouchInterfacePrivate;
};

} // namespace KWin

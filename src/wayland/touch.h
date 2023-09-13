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

namespace KWaylandServer
{
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

    SurfaceInterface *focusedSurface() const;

    void sendDown(qint32 id, quint32 serial, const QPointF &localPos, SurfaceInterface *surface);
    void sendUp(qint32 id, quint32 serial);
    void sendFrame();
    void sendCancel();
    void sendMotion(qint32 id, const QPointF &localPos);

private:
    explicit TouchInterface(SeatInterface *seat);
    std::unique_ptr<TouchInterfacePrivate> d;

    friend class SeatInterfacePrivate;
    friend class TouchInterfacePrivate;
};

} // namespace KWaylandServer

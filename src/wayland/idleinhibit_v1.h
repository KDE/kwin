/*
    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class IdleInhibitManagerV1InterfacePrivate;

/**
 * The IdleInhibitorManagerInterface is used by clients to inhibit idle on a
 * SurfaceInterface. Whether a SurfaceInterface inhibits idle is exposes through
 * @link{SurfaceInterface::inhibitsIdle}.
 */
class KWIN_EXPORT IdleInhibitManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit IdleInhibitManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~IdleInhibitManagerV1Interface() override;

private:
    std::unique_ptr<IdleInhibitManagerV1InterfacePrivate> d;
};

}

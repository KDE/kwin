/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include "surface_interface.h"

#include <QObject>

namespace KWaylandServer
{
class CompositorInterfacePrivate;
class Display;

/**
 * The CompositorInterface global allows clients to create surfaces and region objects.
 *
 * The CompositorInterface corresponds to the Wayland interface @c wl_compositor.
 */
class KWIN_EXPORT CompositorInterface : public QObject
{
    Q_OBJECT

public:
    explicit CompositorInterface(Display *display, QObject *parent = nullptr);
    ~CompositorInterface() override;

    /**
     * Returns the Display object for this CompositorInterface.
     */
    Display *display() const;

Q_SIGNALS:
    /**
     * This signal is emitted when a new SurfaceInterface @a surface has been created.
     */
    void surfaceCreated(KWaylandServer::SurfaceInterface *surface);

private:
    std::unique_ptr<CompositorInterfacePrivate> d;
};

} // namespace KWaylandServer

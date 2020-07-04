/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_REGION_INTERFACE_H
#define WAYLAND_SERVER_REGION_INTERFACE_H

#include <QObject>
#include <QRegion>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class CompositorInterface;
class RegionInterfacePrivate;

/**
 * @brief Resource for the wl_region.
 *
 * A RegionInterface gets created by the CompositorInterface and represents
 * a QRegion.
 *
 * @see CompositorInterface
 **/
class KWAYLANDSERVER_EXPORT RegionInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~RegionInterface();

    /**
     * @returns the data of the region as a QRegion.
     **/
    QRegion region() const;

    /**
     * @returns The RegionInterface for the @p native resource.
     **/
    static RegionInterface *get(wl_resource *native);

Q_SIGNALS:
    /**
     * Emitted whenever the region changes.
     **/
    void regionChanged(const QRegion&);

private:
    friend class CompositorInterfacePrivate;
    explicit RegionInterface(CompositorInterface *compositor, wl_resource *resource);
    QScopedPointer<RegionInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::RegionInterface*)

#endif

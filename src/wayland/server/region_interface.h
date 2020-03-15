/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_REGION_INTERFACE_H
#define WAYLAND_SERVER_REGION_INTERFACE_H

#include <QObject>
#include <QRegion>

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"

namespace KWayland
{
namespace Server
{
class CompositorInterface;

/**
 * @brief Resource for the wl_region.
 *
 * A RegionInterface gets created by the CompositorInterface and represents
 * a QRegion.
 *
 * @see CompositorInterface
 **/
class KWAYLANDSERVER_EXPORT RegionInterface : public Resource
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
    friend class CompositorInterface;
    explicit RegionInterface(CompositorInterface *parent, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}
}
Q_DECLARE_METATYPE(KWayland::Server::RegionInterface*)

#endif

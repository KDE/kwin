/*
    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_IDLEINHIBIT_INTERFACE_H
#define KWAYLAND_SERVER_IDLEINHIBIT_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;

/**
 * Enum describing the interface versions the IdleInhibitManagerInterface can support.
 *
 * @since 5.41
 **/
enum class IdleInhibitManagerInterfaceVersion {
    /**
     * zwp_idle_inhibit_manager_v1
     **/
     UnstableV1
};

/**
 * The IdleInhibitorManagerInterface is used by clients to inhibit idle on a
 * SurfaceInterface. Whether a SurfaceInterface inhibits idle is exposes through
 * @link{SurfaceInterface::inhibitsIdle}.
 *
 * @since 5.41
 **/
class KWAYLANDSERVER_EXPORT IdleInhibitManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~IdleInhibitManagerInterface();

    /**
     * @returns The interface version used by this IdleInhibitManagerInterface
     **/
    IdleInhibitManagerInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit IdleInhibitManagerInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};


}
}

#endif

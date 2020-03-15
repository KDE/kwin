/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_RELATIVE_POINTER_INTERFACE_H
#define KWAYLAND_SERVER_RELATIVE_POINTER_INTERFACE_H

#include "global.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;

enum class RelativePointerInterfaceVersion {
    /**
     * zwp_relative_pointer_manager_v1 and zwp_relative_pointer_v1
     **/
    UnstableV1
};

/**
 * Manager object to create relative pointer interfaces.
 *
 * Once created the interaction happens through the SeatInterface class
 * which automatically delegates relative motion events to the created relative pointer
 * interfaces.
 *
 * @see SeatInterface::relativePointerMotion
 * @since 5.28
 **/
class KWAYLANDSERVER_EXPORT RelativePointerManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~RelativePointerManagerInterface();

    /**
     * @returns The interface version used by this RelativePointerManagerInterface
     **/
    RelativePointerInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit RelativePointerManagerInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

}
}

#endif

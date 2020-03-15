/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_POINTERGESTURES_INTERFACE_H
#define KWAYLAND_SERVER_POINTERGESTURES_INTERFACE_H

#include "global.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

/**
 * Enum describing the interface versions the PointerGesturesInterface can support.
 *
 * @since 5.29
 **/
enum class PointerGesturesInterfaceVersion {
    /**
     * zwp_pointer_gestures_v1, zwp_pointer_gesture_swipe_v1 and zwp_pointer_gesture_pinch_v1
     **/
    UnstableV1
};

/**
 * Manager object for the PointerGestures.
 *
 * Creates and manages pointer swipe and pointer pinch gestures which are
 * reported to the SeatInterface.
 *
 * @see Display::createPointerGestures
 * @since 5.29
 **/
class KWAYLANDSERVER_EXPORT PointerGesturesInterface : public Global
{
    Q_OBJECT
public:
    virtual ~PointerGesturesInterface();

    /**
     * @returns The interface version used by this PointerGesturesInterface
     **/
    PointerGesturesInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit PointerGesturesInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

}
}

#endif

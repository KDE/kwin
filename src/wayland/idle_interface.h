/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class IdleInterfacePrivate;

/**
 * @brief Global representing the org_kde_kwin_idle interface.
 *
 * The IdleInterface allows to register callbacks which are invoked if there has
 * not been any user activity (no input) for a specified time span on a seat.
 *
 * A client can bind an idle timeout for a SeatInterface and through that register
 * an idle timeout. The complete interaction is handled internally, thus the API
 * user only needs to create the IdleInterface in order to provide this feature.
 *
 * This interface is useful for clients as it allows them to perform power management,
 * chat applications might want to set to away after no user input for some time, etc.
 *
 * Of course this exposes the global input usage to all clients. Normally clients don't
 * know whether the input devices are used, only if their surfaces have focus. With this
 * interface it is possible to notice that there are input events. A server should consider
 * this to decide whether it wants to provide this feature!
 */
class KWIN_EXPORT IdleInterface : public QObject
{
    Q_OBJECT

public:
    explicit IdleInterface(Display *display, QObject *parent = nullptr);
    ~IdleInterface() override;

private:
    std::unique_ptr<IdleInterfacePrivate> d;
};

}

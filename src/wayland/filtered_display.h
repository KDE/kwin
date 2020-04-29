/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KWAYLAND_SERVER_FILTERED_DISPLAY_H
#define KWAYLAND_SERVER_FILTERED_DISPLAY_H

#include "global.h"
#include "display.h"

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

/**
* Server Implementation that allows one to restrict which globals are available to which clients
*
* Users of this class must implement the virtual @method allowInterface method.
*
* @since 5.FIXME
*/
class KWAYLANDSERVER_EXPORT FilteredDisplay : public Display
{
    Q_OBJECT
public:
    FilteredDisplay(QObject *parent);
    ~FilteredDisplay();

/**
* Return whether the @arg client can see the interface with the given @arg interfaceName
*
* When false will not see these globals for a given interface in the registry,
* and any manual attempts to bind will fail
*
* @return true if the client should be able to access the global with the following interfaceName
*/
    virtual bool allowInterface(ClientConnection *client, const QByteArray &interfaceName) = 0;
private:
    class Private;
    QScopedPointer<Private> d;
};

}

#endif

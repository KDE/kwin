/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_KEYBOARD_INTERFACE_H
#define WAYLAND_SERVER_KEYBOARD_INTERFACE_H

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"

namespace KWayland
{
namespace Server
{

class SeatInterface;
class SurfaceInterface;

/**
 * @brief Resource for the wl_keyboard interface.
 *
 **/
class KWAYLANDSERVER_EXPORT KeyboardInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~KeyboardInterface();

    /**
     * @returns the focused SurfaceInterface on this keyboard resource, if any.
     **/
    SurfaceInterface *focusedSurface() const;

private:
    void setFocusedSurface(SurfaceInterface *surface, quint32 serial);
    void setKeymap(int fd, quint32 size);
    void setKeymap(const QByteArray &content);
    void updateModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial);
    void keyPressed(quint32 key, quint32 serial);
    void keyReleased(quint32 key, quint32 serial);
    void repeatInfo(qint32 charactersPerSecond, qint32 delay);
    friend class SeatInterface;
    explicit KeyboardInterface(SeatInterface *parent, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::KeyboardInterface*)

#endif

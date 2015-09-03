/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015  Marco Martin <mart@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWAYLAND_SERVER_CONTRAST_INTERFACE_H
#define KWAYLAND_SERVER_CONTRAST_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <QObject>
#include <QMarginsF>

#include <KWayland/Server/kwaylandserver_export.h>

struct wl_region;

namespace KWayland
{
namespace Server
{

class BufferInterface;
class Display;

class KWAYLANDSERVER_EXPORT ContrastManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~ContrastManagerInterface();

private:
    explicit ContrastManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

class KWAYLANDSERVER_EXPORT ContrastInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~ContrastInterface();

    QRegion region() const;
    qreal contrast() const;
    qreal intensity() const;
    qreal saturation() const;

private:
    explicit ContrastInterface(ContrastManagerInterface *parent, wl_resource *parentResource);
    friend class ContrastManagerInterface;

    class Private;
    Private *d_func() const;
};

}
}

#endif

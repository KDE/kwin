/****************************************************************************
Copyright 2015  Marco Martin <notmart@gmail.com>

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
****************************************************************************/
#ifndef KWAYLAND_SERVER_SLIDE_INTERFACE_H
#define KWAYLAND_SERVER_SLIDE_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;

/**
 * TODO
 */
class KWAYLANDSERVER_EXPORT SlideManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~SlideManagerInterface();

private:
    explicit SlideManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

/**
 * TODO
 */
class KWAYLANDSERVER_EXPORT SlideInterface : public Resource
{
    Q_OBJECT
public:
    enum Location {
        Left = 0, /**< Slide from the left edge of the screen */
        Top, /**< Slide from the top edge of the screen */
        Right, /**< Slide from the bottom edge of the screen */
        Bottom /**< Slide from the bottom edge of the screen */
    };

    virtual ~SlideInterface();

    /**
     * @returns the location the window will be slided from
     */
    Location location() const;

    /**
     * @returns the offset from the screen edge the window will
     *          be slided from
     */
    qint32 offset() const;

private:
    explicit SlideInterface(SlideManagerInterface *parent, wl_resource *parentResource);
    friend class SlideManagerInterface;

    class Private;
    Private *d_func() const;
};


}
}

#endif

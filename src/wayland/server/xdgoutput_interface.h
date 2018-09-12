/****************************************************************************
Copyright 2018  David Edmundson <kde@davidedmundson.co.uk>

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
#ifndef KWAYLAND_SERVER_XDGOUTPUT_INTERFACE_H
#define KWAYLAND_SERVER_XDGOUTPUT_INTERFACE_H

#include "global.h"
#include "resource.h"


#include <KWayland/Server/kwaylandserver_export.h>


/*
 * In terms of protocol XdgOutputInterface are a resource
 * but for the sake of sanity, we should treat XdgOutputs as globals like Output is
 * Hence this doesn't match most of kwayland API paradigms.
 */

namespace KWayland
{
namespace Server
{

class Display;
class OutputInterface;
class XdgOutputInterface;

/**
 * Global manager for XdgOutputs
 * @since 5.47
 */
class KWAYLANDSERVER_EXPORT XdgOutputManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~XdgOutputManagerInterface();
    /**
     * Creates an XdgOutputInterface object for an existing Output
     * which exposes XDG specific properties of outputs
     *
     * @arg output the wl_output interface this XDG output is for
     * @parent the parent of the newly created object
     */
    XdgOutputInterface* createXdgOutput(OutputInterface *output, QObject *parent);
private:
    explicit XdgOutputManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
    Private *d_func() const;
};

/**
 * Extension to Output
 * Users should set all relevant values on creation and on future changes.
 * done() should be explicitly called after change batches including initial setting.
 * @since 5.47
 */
class KWAYLANDSERVER_EXPORT XdgOutputInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~XdgOutputInterface();

    /**
     * Sets the size of this output in logical co-ordinates.
     * Users should call done() after setting all values
     */
    void setLogicalSize(const QSize &size);

    /**
     * Returns the last set logical size on this output
     */
    QSize logicalSize() const;

    /**
     * Sets the topleft position of this output in logical co-ordinates.
     * Users should call done() after setting all values
     * @see OutputInterface::setPosition
     */
    void setLogicalPosition(const QPoint &pos);

    /**
     * Returns the last set logical position on this output
     */
    QPoint logicalPosition() const;

    /**
     * Submit changes to all clients
     */
    void done();

private:
    explicit XdgOutputInterface(QObject *parent);
    friend class XdgOutputManagerInterface;

    class Private;
    QScopedPointer<Private> d;
};


}
}

#endif

/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
     * @brief Sets a short name of the output
     * This should be consistent across reboots for the same monitor
     * It should be set once before the first done call
     * @since 5.XDGOUTPUT
     */
    void setName(const QString &name);
    /**
     * The last set name
     * @since 5.XDGOUTPUT
     */
    void name() const;

    /**
     * @brief Sets a longer description of the output
     * This should be consistent across reboots for the same monitor
     * It should be set once before the first done call
     * @since 5.XDGOUTPUT
     */
    void setDescription(const QString &description);
    /**
     * The last set description
     * @since 5.XDGOUTPUT
     */
    void description() const;

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

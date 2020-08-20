/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2020 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_XDGOUTPUT_INTERFACE_H
#define KWAYLAND_SERVER_XDGOUTPUT_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>


/*
 * In terms of protocol XdgOutputInterface are a resource
 * but for the sake of sanity, we should treat XdgOutputs as globals like Output is
 * Hence this doesn't match most of kwayland API paradigms.
 */

namespace KWaylandServer
{

class Display;
class OutputInterface;
class XdgOutputV1Interface;

class XdgOutputManagerV1InterfacePrivate;
class XdgOutputV1InterfacePrivate;

/**
 * Global manager for XdgOutputs
 */
class KWAYLANDSERVER_EXPORT XdgOutputManagerV1Interface : public QObject
{
    Q_OBJECT
public:
    ~XdgOutputManagerV1Interface() override;
    /**
     * Creates an XdgOutputInterface object for an existing Output
     * which exposes XDG specific properties of outputs
     *
     * @arg output the wl_output interface this XDG output is for
     * @parent the parent of the newly created object
     */
    XdgOutputV1Interface* createXdgOutput(OutputInterface *output, QObject *parent);
private:
    explicit XdgOutputManagerV1Interface(Display *display, QObject *parent = nullptr);
    friend class Display;
    QScopedPointer<XdgOutputManagerV1InterfacePrivate> d;
};

/**
 * Extension to Output
 * Users should set all relevant values on creation and on future changes.
 * done() should be explicitly called after change batches including initial setting.
 */
class KWAYLANDSERVER_EXPORT XdgOutputV1Interface : public QObject
{
    Q_OBJECT
public:
    ~XdgOutputV1Interface() override;

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
     */
    void setName(const QString &name);
    /**
     * The last set name
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
     */
    void description() const;

    /**
     * Submit changes to all clients
     */
    void done();

private:
    explicit XdgOutputV1Interface(QObject *parent);
    friend class XdgOutputManagerV1Interface;
    friend class XdgOutputManagerV1InterfacePrivate;

    QScopedPointer<XdgOutputV1InterfacePrivate> d;
};


}

#endif

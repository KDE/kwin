/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2020 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_XDGOUTPUT_INTERFACE_H
#define KWAYLAND_SERVER_XDGOUTPUT_INTERFACE_H

#include "kwin_export.h"

#include <QObject>
#include <memory>

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
class KWIN_EXPORT XdgOutputManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit XdgOutputManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgOutputManagerV1Interface() override;
    /**
     * Creates an XdgOutputInterface object for an existing Output
     * which exposes XDG specific properties of outputs
     *
     * @arg output the wl_output interface this XDG output is for
     * @parent the parent of the newly created object
     */
    XdgOutputV1Interface *createXdgOutput(OutputInterface *output, QObject *parent);

private:
    std::unique_ptr<XdgOutputManagerV1InterfacePrivate> d;
};

/**
 * Extension to Output
 * Users should set all relevant values on creation and on future changes.
 * done() should be explicitly called after change batches including initial setting.
 */
class KWIN_EXPORT XdgOutputV1Interface : public QObject
{
    Q_OBJECT
public:
    ~XdgOutputV1Interface() override;

private:
    void resend();
    void update();

    explicit XdgOutputV1Interface(OutputInterface *output, QObject *parent);
    friend class XdgOutputV1InterfacePrivate;
    friend class XdgOutputManagerV1Interface;
    friend class XdgOutputManagerV1InterfacePrivate;

    std::unique_ptr<XdgOutputV1InterfacePrivate> d;
};

}

#endif

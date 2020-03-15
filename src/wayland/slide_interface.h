/*
    SPDX-FileCopyrightText: 2015 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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

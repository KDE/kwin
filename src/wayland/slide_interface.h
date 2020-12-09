/*
    SPDX-FileCopyrightText: 2015 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_SLIDE_INTERFACE_H
#define KWAYLAND_SERVER_SLIDE_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class SlideManagerInterfacePrivate;
class SlideInterfacePrivate;

class KWAYLANDSERVER_EXPORT SlideManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit SlideManagerInterface(Display *display, QObject *parent = nullptr);
    ~SlideManagerInterface() override;

private:
    QScopedPointer<SlideManagerInterfacePrivate> d;
};

class KWAYLANDSERVER_EXPORT SlideInterface : public QObject
{
    Q_OBJECT
public:
    enum Location {
        Left = 0, /**< Slide from the left edge of the screen */
        Top = 1, /**< Slide from the top edge of the screen */
        Right = 2, /**< Slide from the bottom edge of the screen */
        Bottom = 3 /**< Slide from the bottom edge of the screen */
    };

    ~SlideInterface() override;

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
    explicit SlideInterface(SlideManagerInterface *manager, wl_resource *resource);
    friend class SlideManagerInterfacePrivate;

    QScopedPointer<SlideInterfacePrivate> d;
};


}

#endif

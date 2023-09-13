/*
    SPDX-FileCopyrightText: 2015 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class SlideManagerInterfacePrivate;
class SlideInterfacePrivate;

class KWIN_EXPORT SlideManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit SlideManagerInterface(Display *display, QObject *parent = nullptr);
    ~SlideManagerInterface() override;

    void remove();

private:
    std::unique_ptr<SlideManagerInterfacePrivate> d;
};

class KWIN_EXPORT SlideInterface : public QObject
{
    Q_OBJECT
public:
    enum Location {
        Left = 0, /**< Slide from the left edge of the screen */
        Top = 1, /**< Slide from the top edge of the screen */
        Right = 2, /**< Slide from the bottom edge of the screen */
        Bottom = 3, /**< Slide from the bottom edge of the screen */
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
    explicit SlideInterface(wl_resource *resource);
    friend class SlideManagerInterfacePrivate;

    std::unique_ptr<SlideInterfacePrivate> d;
};
}

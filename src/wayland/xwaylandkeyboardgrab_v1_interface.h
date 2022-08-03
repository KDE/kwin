/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

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
class SeatInterface;
class SurfaceInterface;
class XWaylandKeyboardGrabV1InterfacePrivate;
class XWaylandKeyboardGrabManagerV1InterfacePrivate;

class KWIN_EXPORT XWaylandKeyboardGrabManagerV1Interface : public QObject
{
    Q_OBJECT
public:
    explicit XWaylandKeyboardGrabManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~XWaylandKeyboardGrabManagerV1Interface() override;
    bool hasGrab(SurfaceInterface *surface, SeatInterface *seat) const;

private:
    friend class XWaylandKeyboardGrabManagerV1InterfacePrivate;
    std::unique_ptr<XWaylandKeyboardGrabManagerV1InterfacePrivate> d;
};

class KWIN_EXPORT XWaylandKeyboardGrabV1Interface : public QObject
{
    Q_OBJECT
public:
    ~XWaylandKeyboardGrabV1Interface() override;

private:
    friend class XWaylandKeyboardGrabManagerV1InterfacePrivate;
    XWaylandKeyboardGrabV1Interface(wl_resource *resource);
    std::unique_ptr<XWaylandKeyboardGrabV1InterfacePrivate> d;
};

}

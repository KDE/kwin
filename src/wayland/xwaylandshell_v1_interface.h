/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <kwin_export.h>

#include <QObject>

#include <memory>
#include <optional>

struct wl_client;

namespace KWaylandServer
{

class Display;
class SurfaceInterface;
class XwaylandShellV1Interface;
class XwaylandShellV1InterfacePrivate;
class XwaylandSurfaceV1InterfacePrivate;

class KWIN_EXPORT XwaylandSurfaceV1Interface : public QObject
{
    Q_OBJECT

public:
    XwaylandSurfaceV1Interface(XwaylandShellV1Interface *shell, SurfaceInterface *surface, wl_client *client, uint32_t id, int version);
    ~XwaylandSurfaceV1Interface() override;

    SurfaceInterface *surface() const;
    std::optional<uint64_t> serial() const;

private:
    std::unique_ptr<XwaylandSurfaceV1InterfacePrivate> d;
};

class KWIN_EXPORT XwaylandShellV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit XwaylandShellV1Interface(Display *display, QObject *parent = nullptr);
    ~XwaylandShellV1Interface() override;

    XwaylandSurfaceV1Interface *findSurface(uint64_t serial) const;

Q_SIGNALS:
    void surfaceAssociated(XwaylandSurfaceV1Interface *surface);

private:
    std::unique_ptr<XwaylandShellV1InterfacePrivate> d;
};

} // namespace KWaylandServer

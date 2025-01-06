/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QPointF>

struct wl_resource;

namespace KWin
{

class CursorImageManagerV1InterfacePrivate;
class CursorImageV1Private;
class Display;
class SurfaceInterface;
class SurfaceRole;

class KWIN_EXPORT CursorImageV1 : public QObject
{
    Q_OBJECT

public:
    CursorImageV1(SurfaceInterface *surface, wl_resource *resource);
    ~CursorImageV1() override;

    QPointF hotspot() const;
    SurfaceInterface *surface() const;

    static SurfaceRole *role();
    static CursorImageV1 *get(wl_resource *resource);

Q_SIGNALS:
    void hotspotChanged();

private:
    std::unique_ptr<CursorImageV1Private> d;
};

class KWIN_EXPORT CursorImageManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit CursorImageManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~CursorImageManagerV1Interface() override;

private:
    std::unique_ptr<CursorImageManagerV1InterfacePrivate> d;
};

} // namespace KWin

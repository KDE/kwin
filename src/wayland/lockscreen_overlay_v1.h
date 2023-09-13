/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QVector>
#include <functional>
#include <memory>
#include <optional>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class SurfaceInterface;

class LockscreenOverlayV1InterfacePrivate;

class KWIN_EXPORT LockscreenOverlayV1Interface : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(LockscreenOverlayV1Interface)
public:
    explicit LockscreenOverlayV1Interface(Display *display, QObject *parent = nullptr);
    ~LockscreenOverlayV1Interface() override;

Q_SIGNALS:
    /// Notifies about the @p surface being activated
    void allowRequested(SurfaceInterface *surface);

private:
    friend class LockscreenOverlayV1InterfacePrivate;
    LockscreenOverlayV1Interface(LockscreenOverlayV1Interface *parent);
    std::unique_ptr<LockscreenOverlayV1InterfacePrivate> d;
};

}

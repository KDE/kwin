/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "kwin_export.h"

#include <QObject>
#include <QRect>

#include <memory>

struct wl_client;

namespace KWin
{

class BlurManagerV1InterfacePrivate;
class BlurV1InterfacePrivate;
class Display;
class SurfaceInterface;

class KWIN_EXPORT BlurManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit BlurManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~BlurManagerV1Interface() override;

    void remove();

private:
    std::unique_ptr<BlurManagerV1InterfacePrivate> d;
};

struct BlurMaskV1
{
    GraphicsBufferRef mask;
    int scale;
    QRect center;
};

class KWIN_EXPORT BlurV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit BlurV1Interface(SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version);
    ~BlurV1Interface() override;

    std::optional<BlurMaskV1> mask() const;
    std::optional<QRegion> shape() const;

private:
    std::unique_ptr<BlurV1InterfacePrivate> d;
};

} // namespace KWin

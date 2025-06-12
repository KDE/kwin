/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "effect/globals.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{

class Display;
class ScreenEdgeManagerV2InterfacePrivate;
class ScreenEdgeV2Interface;
class ScreenEdgeV2InterfacePrivate;
class SurfaceInterface;

class KWIN_EXPORT ScreenEdgeManagerV2Interface : public QObject
{
    Q_OBJECT

public:
    explicit ScreenEdgeManagerV2Interface(Display *display, QObject *parent = nullptr);
    ~ScreenEdgeManagerV2Interface() override;

Q_SIGNALS:
    void edgeRequested(ScreenEdgeV2Interface *edge);

private:
    std::unique_ptr<ScreenEdgeManagerV2InterfacePrivate> d;
};

class KWIN_EXPORT ScreenEdgeV2Interface : public QObject
{
    Q_OBJECT

public:
    ScreenEdgeV2Interface(SurfaceInterface *surface, ElectricBorder border, wl_resource *resource);
    ~ScreenEdgeV2Interface() override;

    SurfaceInterface *surface() const;
    ElectricBorder border() const;

Q_SIGNALS:
    void showRequested();
    void hideRequested();

private:
    std::unique_ptr<ScreenEdgeV2InterfacePrivate> d;
};

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QMarginsF>
#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{

class GraphicsBuffer;
class Display;
class ShadowManagerInterfacePrivate;
class ShadowInterfacePrivate;

class KWIN_EXPORT ShadowManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit ShadowManagerInterface(Display *display, QObject *parent = nullptr);
    ~ShadowManagerInterface() override;

private:
    std::unique_ptr<ShadowManagerInterfacePrivate> d;
};

class KWIN_EXPORT ShadowInterface : public QObject
{
    Q_OBJECT
public:
    ~ShadowInterface() override;

    GraphicsBuffer *left() const;
    GraphicsBuffer *topLeft() const;
    GraphicsBuffer *top() const;
    GraphicsBuffer *topRight() const;
    GraphicsBuffer *right() const;
    GraphicsBuffer *bottomRight() const;
    GraphicsBuffer *bottom() const;
    GraphicsBuffer *bottomLeft() const;

    QMarginsF offset() const;

private:
    explicit ShadowInterface(ShadowManagerInterface *manager, wl_resource *resource);
    friend class ShadowManagerInterfacePrivate;

    std::unique_ptr<ShadowInterfacePrivate> d;
};

}

/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_SHADOW_INTERFACE_H
#define KWAYLAND_SERVER_SHADOW_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <QObject>
#include <QMarginsF>

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class BufferInterface;
class Display;

/**
 * TODO
 */
class KWAYLANDSERVER_EXPORT ShadowManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~ShadowManagerInterface();

private:
    explicit ShadowManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

/**
 * TODO
 */
class KWAYLANDSERVER_EXPORT ShadowInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~ShadowInterface();

    BufferInterface *left() const;
    BufferInterface *topLeft() const;
    BufferInterface *top() const;
    BufferInterface *topRight() const;
    BufferInterface *right() const;
    BufferInterface *bottomRight() const;
    BufferInterface *bottom() const;
    BufferInterface *bottomLeft() const;

    QMarginsF offset() const;

private:
    explicit ShadowInterface(ShadowManagerInterface *parent, wl_resource *parentResource);
    friend class ShadowManagerInterface;

    class Private;
    Private *d_func() const;
};

}
}

#endif

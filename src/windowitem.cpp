/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowitem.h"
#include "abstract_client.h"
#include "decorationitem.h"
#include "shadowitem.h"
#include "surfaceitem_internal.h"
#include "surfaceitem_wayland.h"
#include "surfaceitem_x11.h"

namespace KWin
{

WindowItem::WindowItem(Scene::Window *window, Item *parent)
    : Item(window, parent)
{
    AbstractClient *client = qobject_cast<AbstractClient *>(window->window());
    if (client) {
        connect(client, &AbstractClient::decorationChanged, this, &WindowItem::updateDecorationItem);
        updateDecorationItem();
    }
}

SurfaceItem *WindowItem::surfaceItem() const
{
    return m_surfaceItem.data();
}

DecorationItem *WindowItem::decorationItem() const
{
    return m_decorationItem.data();
}

ShadowItem *WindowItem::shadowItem() const
{
    return m_shadowItem.data();
}

void WindowItem::updateSurfaceItem(SurfaceItem *surfaceItem)
{
    Toplevel *toplevel = window()->window();

    m_surfaceItem.reset(surfaceItem);

    connect(toplevel, &Toplevel::bufferGeometryChanged, this, &WindowItem::updateSurfacePosition);
    connect(toplevel, &Toplevel::frameGeometryChanged, this, &WindowItem::updateSurfacePosition);

    updateSurfacePosition();
}

void WindowItem::updateSurfacePosition()
{
    const Toplevel *toplevel = window()->window();

    const QRect bufferGeometry = toplevel->bufferGeometry();
    const QRect frameGeometry = toplevel->frameGeometry();

    m_surfaceItem->setPosition(bufferGeometry.topLeft() - frameGeometry.topLeft());
}

void WindowItem::setShadow(Shadow *shadow)
{
    if (shadow) {
        if (!m_shadowItem || m_shadowItem->shadow() != shadow) {
            m_shadowItem.reset(new ShadowItem(shadow, window(), this));
        }
        if (m_decorationItem) {
            m_shadowItem->stackBefore(m_decorationItem.data());
        } else if (m_surfaceItem) {
            m_shadowItem->stackBefore(m_decorationItem.data());
        }
    } else {
        m_shadowItem.reset();
    }
}

void WindowItem::updateDecorationItem()
{
    AbstractClient *client = qobject_cast<AbstractClient *>(window()->window());
    if (!client || client->isZombie()) {
        return;
    }
    if (client->decoration()) {
        m_decorationItem.reset(new DecorationItem(client->decoration(), window(), this));
        if (m_shadowItem) {
            m_decorationItem->stackAfter(m_shadowItem.data());
        } else if (m_surfaceItem) {
            m_decorationItem->stackBefore(m_surfaceItem.data());
        }
    } else {
        m_decorationItem.reset();
    }
}

WindowItemX11::WindowItemX11(Scene::Window *window, Item *parent)
    : WindowItem(window, parent)
{
    Toplevel *toplevel = window->window();

    switch (kwinApp()->operationMode()) {
    case Application::OperationModeX11:
        initialize();
        break;
    case Application::OperationModeXwayland:
        // Xwayland windows and Wayland surfaces are associated asynchronously.
        if (toplevel->surface()) {
            initialize();
        } else {
            connect(toplevel, &Toplevel::surfaceChanged, this, &WindowItemX11::initialize);
        }
        break;
    case Application::OperationModeWaylandOnly:
        Q_UNREACHABLE();
    }
}

void WindowItemX11::initialize()
{
    if (!window()->window()->surface()) {
        updateSurfaceItem(new SurfaceItemX11(window(), this));
    } else {
        updateSurfaceItem(new SurfaceItemXwayland(window(), this));
    }
}

WindowItemWayland::WindowItemWayland(Scene::Window *window, Item *parent)
    : WindowItem(window, parent)
{
    Toplevel *toplevel = window->window();
    updateSurfaceItem(new SurfaceItemWayland(toplevel->surface(), window, this));
}

WindowItemInternal::WindowItemInternal(Scene::Window *window, Item *parent)
    : WindowItem(window, parent)
{
    updateSurfaceItem(new SurfaceItemInternal(window, this));
}

} // namespace KWin

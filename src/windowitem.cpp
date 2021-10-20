/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowitem.h"
#include "abstract_client.h"
#include "decorationitem.h"
#include "deleted.h"
#include "shadowitem.h"
#include "surfaceitem.h"

namespace KWin
{

WindowItem::WindowItem(Toplevel *window, Item *parent)
    : Item(parent)
    , m_window(window)
{
    AbstractClient *client = qobject_cast<AbstractClient *>(window);
    if (client) {
        connect(client, &AbstractClient::decorationChanged, this, &WindowItem::updateDecorationItem);
        updateDecorationItem();
    }

    connect(window, &Toplevel::shadowChanged, this, &WindowItem::updateShadowItem);
    updateShadowItem();

    if (window->sceneSurface()) {
        updateSurfaceItem();
    } else {
        connect(window, &Toplevel::sceneSurfaceChanged, this, &WindowItem::updateSurfaceItem);
    }

    connect(window, &Toplevel::windowClosed, this, &WindowItem::handleWindowClosed);
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

Toplevel *WindowItem::window() const
{
    return m_window;
}

void WindowItem::handleWindowClosed(Toplevel *original, Deleted *deleted)
{
    Q_UNUSED(original)
    m_window = deleted;
}

void WindowItem::updateSurfaceItem()
{
    m_surfaceItem.reset(new SurfaceItem(m_window->sceneSurface(), this));

    if (m_surfaceItem) {
        connect(m_window, &Toplevel::shadeChanged, this, &WindowItem::updateSurfaceVisibility);
        connect(m_window, &Toplevel::bufferGeometryChanged, this, &WindowItem::updateSurfacePosition);
        connect(m_window, &Toplevel::frameGeometryChanged, this, &WindowItem::updateSurfacePosition);

        updateSurfacePosition();
        updateSurfaceVisibility();
    } else {
        disconnect(m_window, &Toplevel::shadeChanged, this, &WindowItem::updateSurfaceVisibility);
        disconnect(m_window, &Toplevel::bufferGeometryChanged, this, &WindowItem::updateSurfacePosition);
        disconnect(m_window, &Toplevel::frameGeometryChanged, this, &WindowItem::updateSurfacePosition);
    }
}

void WindowItem::updateSurfacePosition()
{
    const QRect bufferGeometry = m_window->bufferGeometry();
    const QRect frameGeometry = m_window->frameGeometry();

    m_surfaceItem->setPosition(bufferGeometry.topLeft() - frameGeometry.topLeft());
}

void WindowItem::updateSurfaceVisibility()
{
    m_surfaceItem->setVisible(!m_window->isShade());
}

void WindowItem::updateShadowItem()
{
    Shadow *shadow = m_window->shadow();
    if (shadow) {
        if (!m_shadowItem || m_shadowItem->shadow() != shadow) {
            m_shadowItem.reset(new ShadowItem(shadow, m_window, this));
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
    AbstractClient *client = qobject_cast<AbstractClient *>(m_window);
    if (!client || client->isZombie()) {
        return;
    }
    if (client->decoration()) {
        m_decorationItem.reset(new DecorationItem(client->decoration(), client, this));
        if (m_shadowItem) {
            m_decorationItem->stackAfter(m_shadowItem.data());
        } else if (m_surfaceItem) {
            m_decorationItem->stackBefore(m_surfaceItem.data());
        }
    } else {
        m_decorationItem.reset();
    }
}

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem.h"

namespace KWin
{

SurfaceItem::SurfaceItem(Scene::Window *window, Item *parent)
    : Item(window, parent)
{
}

QPointF SurfaceItem::mapToWindow(const QPointF &point) const
{
    return rootPosition() + point - window()->pos();
}

QRegion SurfaceItem::shape() const
{
    return QRegion();
}

QRegion SurfaceItem::opaque() const
{
    return QRegion();
}

void SurfaceItem::addDamage(const QRegion &region)
{
    m_damage += region;
    scheduleRepaint(region);

    Toplevel *toplevel = window()->window();
    emit toplevel->damaged(toplevel, region);
}

void SurfaceItem::resetDamage()
{
    m_damage = QRegion();
}

QRegion SurfaceItem::damage() const
{
    return m_damage;
}

WindowPixmap *SurfaceItem::windowPixmap() const
{
    if (m_windowPixmap && m_windowPixmap->isValid()) {
        return m_windowPixmap.data();
    }
    if (m_previousWindowPixmap && m_previousWindowPixmap->isValid()) {
        return m_previousWindowPixmap.data();
    }
    return nullptr;
}

WindowPixmap *SurfaceItem::previousWindowPixmap() const
{
    return m_previousWindowPixmap.data();
}

void SurfaceItem::referencePreviousPixmap()
{
    if (m_previousWindowPixmap && m_previousWindowPixmap->isDiscarded()) {
        m_referencePixmapCounter++;
    }
}

void SurfaceItem::unreferencePreviousPixmap()
{
    if (!m_previousWindowPixmap || !m_previousWindowPixmap->isDiscarded()) {
        return;
    }
    m_referencePixmapCounter--;
    if (m_referencePixmapCounter == 0) {
        m_previousWindowPixmap.reset();
    }
}

void SurfaceItem::updatePixmap()
{
    if (m_windowPixmap.isNull()) {
        m_windowPixmap.reset(createPixmap());
    }
    if (m_windowPixmap->isValid()) {
        m_windowPixmap->update();
    } else {
        m_windowPixmap->create();
        if (m_windowPixmap->isValid()) {
            if (m_referencePixmapCounter == 0) {
                m_previousWindowPixmap.reset();
            }
            discardQuads();
        }
    }
}

void SurfaceItem::discardPixmap()
{
    if (!m_windowPixmap.isNull()) {
        if (m_windowPixmap->isValid()) {
            m_previousWindowPixmap.reset(m_windowPixmap.take());
            m_previousWindowPixmap->markAsDiscarded();
        } else {
            m_windowPixmap.reset();
        }
    }
    addDamage(rect());
}

void SurfaceItem::preprocess()
{
    updatePixmap();
}

} // namespace KWin

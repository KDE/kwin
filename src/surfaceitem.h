/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "item.h"

namespace KWin
{

/**
 * The SurfaceItem class represents a surface with some contents.
 */
class KWIN_EXPORT SurfaceItem : public Item
{
    Q_OBJECT

public:
    QPointF mapToWindow(const QPointF &point) const;
    virtual QPointF mapToBuffer(const QPointF &point) const = 0;

    virtual QRegion shape() const;
    virtual QRegion opaque() const;

    void addDamage(const QRegion &region);
    void resetDamage();
    QRegion damage() const;

    WindowPixmap *windowPixmap() const;
    WindowPixmap *previousWindowPixmap() const;

    void referencePreviousPixmap();
    void unreferencePreviousPixmap();

protected:
    explicit SurfaceItem(Scene::Window *window, Item *parent = nullptr);

    virtual WindowPixmap *createPixmap() = 0;
    void preprocess() override;

    void discardPixmap();
    void updatePixmap();

    QRegion m_damage;
    QScopedPointer<WindowPixmap> m_windowPixmap;
    QScopedPointer<WindowPixmap> m_previousWindowPixmap;
    int m_referencePixmapCounter = 0;

    friend class Scene::Window;
};

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "item.h"
#include "surface.h"

namespace KWin
{

class Deleted;
class Toplevel;

/**
 * The SurfaceItem class represents a surface with some contents.
 */
class KWIN_EXPORT SurfaceItem : public Item, public SurfaceView
{
    Q_OBJECT

public:
    explicit SurfaceItem(Surface *surface, Item *parent = nullptr);

    QRegion shape() const;
    QRegion opaque() const;

    void addDamage(const QRegion &region);
    void resetDamage();
    QRegion damage() const;

    void discardPixmap();
    void updatePixmap();

    SurfacePixmap *pixmap() const;
    SurfacePixmap *previousPixmap() const;

    void referencePreviousPixmap();
    void unreferencePreviousPixmap();

private Q_SLOTS:
    void handlePositionChanged();
    void handleSizeChanged();
    void handleBelowChanged();
    void handleAboveChanged();

protected:
    void surfaceFrame() override;
    void surfacePixmapInvalidated() override;
    void surfaceQuadsInvalidated() override;

    void preprocess() override;
    WindowQuadList buildQuads() const override;

private:
    SurfaceItem *getOrCreateSubSurfaceItem(Surface *subsurface);

    QRegion m_damage;
    QScopedPointer<SurfacePixmap> m_pixmap;
    QScopedPointer<SurfacePixmap> m_previousPixmap;
    int m_referencePixmapCounter = 0;
    QHash<Surface *, SurfaceItem *> m_subsurfaces;
};

} // namespace KWin

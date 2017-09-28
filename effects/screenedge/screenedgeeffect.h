/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_SCREEN_EDGE_EFFECT_H
#define KWIN_SCREEN_EDGE_EFFECT_H
#include <kwineffects.h>

class QTimer;
namespace Plasma {
    class Svg;
}

namespace KWin {
class Glow;
class GLTexture;

class ScreenEdgeEffect : public Effect
{
    Q_OBJECT
public:
    ScreenEdgeEffect();
    virtual ~ScreenEdgeEffect();
    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData &data);
    virtual bool isActive() const;

    int requestedEffectChainPosition() const override {
        return 90;
    }

private Q_SLOTS:
    void edgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry);
    void cleanup();
private:
    void ensureGlowSvg();
    Glow *createGlow(ElectricBorder border, qreal factor, const QRect &geometry);
    template <typename T>
    T *createCornerGlow(ElectricBorder border);
    template <typename T>
    T *createEdgeGlow(ElectricBorder border, const QSize &size);
    QSize cornerGlowSize(ElectricBorder border);
    Plasma::Svg *m_glow = nullptr;
    QHash<ElectricBorder, Glow*> m_borders;
    QTimer *m_cleanupTimer;
};

class Glow
{
public:
    QScopedPointer<GLTexture> texture;
    QScopedPointer<QImage> image;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    QScopedPointer<XRenderPicture> picture;
#endif
    QSize pictureSize;
    qreal strength;
    QRect geometry;
    ElectricBorder border;
};

}

#endif

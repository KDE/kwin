/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#ifndef BLUR_H
#define BLUR_H

#include <kwineffects.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>

#include <QVector>
#include <QVector2D>

namespace KWin
{

class BlurShader;

class BlurEffect : public KWin::Effect
{
    Q_OBJECT
public:
    BlurEffect();
    ~BlurEffect();

    static bool supported();
    static bool enabledByDefault();

    void reconfigure(ReconfigureFlags flags);
    void prePaintScreen(ScreenPrePaintData &data, int time);
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    void drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data);
    void paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity);

public Q_SLOTS:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDeleted(EffectWindow *w);
    void slotPropertyNotify(EffectWindow *w, long atom);
    void slotScreenGeometryChanged();

private:
    QRect expand(const QRect &rect) const;
    QRegion expand(const QRegion &region) const;
    QRegion blurRegion(const EffectWindow *w) const;
    bool shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const;
    void updateBlurRegion(EffectWindow *w) const;
    void drawRegion(const QRegion &region);
    void doBlur(const QRegion &shape, const QRect &screen, const float opacity);
    void doCachedBlur(EffectWindow *w, const QRegion& region, const float opacity);

private:
    BlurShader *shader;
    QVector<QVector2D> vertices;
    GLRenderTarget *target;
    GLTexture tex;
    long net_wm_blur_region;
    QRegion m_damagedArea; // keeps track of the area which has been damaged (from bottom to top)
    QRegion m_paintedArea; // actually painted area which is greater than m_damagedArea
    QRegion m_currentBlur; // keeps track of the currently blured area of non-caching windows(from bottom to top)
    bool m_shouldCache;

    struct BlurWindowInfo {
        GLTexture blurredBackground; // keeps the horizontally blurred background
        QRegion damagedRegion;
        bool dropCache;
    };

    QHash< const EffectWindow*, BlurWindowInfo > windows;
};

} // namespace KWin

#endif


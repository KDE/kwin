/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *   Copyright 2014 Marco Martin <mart@kde.org>
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

#ifndef CONTRAST_H
#define CONTRAST_H

#include <kwineffects.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>

#include <QVector>
#include <QVector2D>

namespace KWayland
{
namespace Server
{
class ContrastManagerInterface;
}
}

namespace KWin
{

class ContrastShader;

class ContrastEffect : public KWin::Effect
{
    Q_OBJECT
public:
    ContrastEffect();
    ~ContrastEffect();

    static bool supported();
    static bool enabledByDefault();

    static QMatrix4x4 colorMatrix(qreal contrast, qreal intensity, qreal saturation);
    void reconfigure(ReconfigureFlags flags);
    void prePaintScreen(ScreenPrePaintData &data, int time);
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    void drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data);
    void paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity);

    virtual bool provides(Feature feature);

    int requestedEffectChainPosition() const override {
        return 76;
    }

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotPropertyNotify(KWin::EffectWindow *w, long atom);
    void slotScreenGeometryChanged();

private:
    QRegion contrastRegion(const EffectWindow *w) const;
    bool shouldContrast(const EffectWindow *w, int mask, const WindowPaintData &data) const;
    void updateContrastRegion(EffectWindow *w);
    void doContrast(EffectWindow *w, const QRegion &shape, const QRect &screen, const float opacity, const QMatrix4x4 &screenProjection);
    void uploadRegion(QVector2D *&map, const QRegion &region);
    void uploadGeometry(GLVertexBuffer *vbo, const QRegion &region);

private:
    ContrastShader *shader;
    long net_wm_contrast_region;
    QRegion m_paintedArea; // actually painted area which is greater than m_damagedArea
    QRegion m_currentContrast; // keeps track of the currently contrasted area of non-caching windows(from bottom to top)
    QHash< const EffectWindow*, QMatrix4x4> m_colorMatrices;
    QHash< const EffectWindow*, QMetaObject::Connection > m_contrastChangedConnections; // used only in Wayland to keep track of effect changed
    KWayland::Server::ContrastManagerInterface *m_contrastManager = nullptr;
};

inline
bool ContrastEffect::provides(Effect::Feature feature)
{
    if (feature == Contrast) {
        return true;
    }
    return KWin::Effect::provides(feature);
}


} // namespace KWin

#endif


/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/timeline.h"

#include <memory>

class QAction;

namespace KWin
{

class GLFramebuffer;
class GLTexture;

class MagnifierEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(QSize magnifierSize READ magnifierSize)
    Q_PROPERTY(qreal targetZoom READ targetZoom)
public:
    MagnifierEffect();
    ~MagnifierEffect() override;
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;
    bool isActive() const override;
    static bool supported();

    // for properties
    QSize magnifierSize() const;
    qreal targetZoom() const;
private Q_SLOTS:
    void saveInitialZoom();
    void zoomIn();
    void zoomOut();
    void toggle();
    void slotMouseChanged(const QPointF &pos, const QPointF &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDamaged();

private:
    QRect magnifierArea(QPointF pos = cursorPos()) const;
    QRect visibleArea(QPointF pos = cursorPos()) const;
    void setTargetZoom(double zoomFactor);
    void realtimeZoom(double delta);

    std::unique_ptr<QTimer> m_configurationTimer;
    double m_zoom;
    double m_targetZoom;
    double m_zoomFactor;
    double m_pixelGridZoom;
    std::unique_ptr<QAction> m_zoomInAxisAction;
    std::unique_ptr<QAction> m_zoomOutAxisAction;
    Qt::KeyboardModifiers m_axisModifiers;
    std::unique_ptr<QAction> m_touchpadAction;
    double m_lastPinchProgress = 0;
    AnimationClock m_clock;
    QSize m_magnifierSize;
    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_fbo;
};

} // namespace

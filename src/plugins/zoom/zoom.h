/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "config-kwin.h"

#include "core/colorspace.h"
#include "effect/effect.h"

#include <QTime>
#include <QTimeLine>

namespace KWin
{

#if HAVE_ACCESSIBILITY
class ZoomAccessibilityIntegration;
#endif

class GLFramebuffer;
class GLTexture;
class GLVertexBuffer;
class GLShader;

class ZoomEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal zoomFactor READ configuredZoomFactor)
    Q_PROPERTY(int mousePointer READ configuredMousePointer)
    Q_PROPERTY(int mouseTracking READ configuredMouseTracking)
    Q_PROPERTY(bool focusTrackingEnabled READ isFocusTrackingEnabled)
    Q_PROPERTY(bool textCaretTrackingEnabled READ isTextCaretTrackingEnabled)
    Q_PROPERTY(int focusDelay READ configuredFocusDelay)
    Q_PROPERTY(qreal moveFactor READ configuredMoveFactor)
    Q_PROPERTY(qreal targetZoom READ targetZoom)

public:
    ZoomEffect();
    ~ZoomEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen) override;
    void postPaintScreen() override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    // for properties
    qreal configuredZoomFactor() const;
    int configuredMousePointer() const;
    int configuredMouseTracking() const;
    bool isFocusTrackingEnabled() const;
    bool isTextCaretTrackingEnabled() const;
    int configuredFocusDelay() const;
    qreal configuredMoveFactor() const;
    qreal targetZoom() const;

private Q_SLOTS:
    void zoomIn();
    void zoomTo(double to);
    void zoomOut();
    void actualSize();
    void moveZoomLeft();
    void moveZoomRight();
    void moveZoomUp();
    void moveZoomDown();
    void moveMouseToFocus();
    void moveMouseToCenter();
    void timelineFrameChanged(int frame);
    void moveFocus(const QPoint &point);
    void slotMouseChanged(const QPointF &pos, const QPointF &old);
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDamaged();
    void slotScreenRemoved(Output *screen);
    void setTargetZoom(double value);

private:
    enum MouseTrackingType {
        MouseTrackingProportional = 0,
        MouseTrackingCentered = 1,
        MouseTrackingPush = 2,
        MouseTrackingDisabled = 3,
    };

    enum MousePointerType {
        MousePointerScale = 0,
        MousePointerKeep = 1,
        MousePointerHide = 2,
    };

    struct OffscreenData
    {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> framebuffer;
        QRectF viewport;
        ColorDescription color = ColorDescription::sRGB;
    };

    void moveZoom(int x, int y);
    bool screenExistsAt(const QPoint &point) const;

    void showCursor();
    void hideCursor();
    GLTexture *ensureCursorTexture();
    OffscreenData *ensureOffscreenData(const RenderTarget &renderTarget, const RenderViewport &viewport, Output *screen);
    void markCursorTextureDirty();

    GLShader *shaderForZoom(double zoom);

#if HAVE_ACCESSIBILITY
    ZoomAccessibilityIntegration *m_accessibilityIntegration = nullptr;
#endif
    double m_zoom = 1.0;
    double m_targetZoom = 1.0;
    double m_sourceZoom = 1.0;
    double m_zoomFactor = 1.25;
    MouseTrackingType m_mouseTracking = MouseTrackingProportional;
    MousePointerType m_mousePointer = MousePointerScale;
    int m_focusDelay = 350; // in milliseconds
    QPoint m_cursorPoint;
    QPoint m_focusPoint;
    QPoint m_prevPoint;
    QTime m_lastMouseEvent;
    QTime m_lastFocusEvent;
    std::unique_ptr<GLTexture> m_cursorTexture;
    bool m_cursorTextureDirty = false;
    bool m_isMouseHidden = false;
    QTimeLine m_timeline;
    int m_xMove = 0;
    int m_yMove = 0;
    double m_moveFactor = 20.0;
    std::chrono::milliseconds m_lastPresentTime = std::chrono::milliseconds::zero();
    std::map<Output *, OffscreenData> m_offscreenData;
    std::unique_ptr<GLShader> m_pixelGridShader;
    double m_pixelGridZoom;
};

} // namespace

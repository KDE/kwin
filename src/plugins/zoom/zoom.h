/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/colorspace.h"
#include "effect/effect.h"
#include "effect/timeline.h"

#include <QAction>
#include <QTime>
#include <QTimeLine>

namespace KWin
{

class CursorItem;
class GLFramebuffer;
class GLTexture;
class GLVertexBuffer;
class GLShader;
class FocusTracker;
class TextCaretTracker;

class ZoomEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal zoomFactor READ configuredZoomFactor)
    Q_PROPERTY(int mousePointer READ configuredMousePointer)
    Q_PROPERTY(int mouseTracking READ configuredMouseTracking)
    Q_PROPERTY(int focusDelay READ configuredFocusDelay)
    Q_PROPERTY(qreal moveFactor READ configuredMoveFactor)
    Q_PROPERTY(qreal targetZoom READ targetZoom)

public:
    ZoomEffect();
    ~ZoomEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    // for properties
    qreal configuredZoomFactor() const;
    int configuredMousePointer() const;
    int configuredMouseTracking() const;
    int configuredFocusDelay() const;
    qreal configuredMoveFactor() const;
    qreal targetZoom() const;

private Q_SLOTS:
    void saveInitialZoom();
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
    void moveFocus(const QPointF &point);
    void slotMouseChanged(const QPointF &pos, const QPointF &old);
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDamaged();
    void slotScreenRemoved(LogicalOutput *screen);
    void setTargetZoom(double value);

private:
    enum MouseTrackingType {
        MouseTrackingProportional = 0,
        MouseTrackingCentered = 1,
        MouseTrackingPush = 2,
        MouseTrackingDisabled = 3,
        MouseTrackingCenteredStrict = 4,
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
        std::shared_ptr<ColorDescription> color = ColorDescription::sRGB;
    };

    void moveZoom(int x, int y);
    bool screenExistsAt(const QPoint &point) const;
    void realtimeZoom(double delta);

    QPointF calculateCursorItemPosition() const;
    void showCursor();
    void hideCursor();
    GLTexture *ensureCursorTexture();
    OffscreenData *ensureOffscreenData(const RenderTarget &renderTarget, const RenderViewport &viewport, LogicalOutput *screen);
    void markCursorTextureDirty();

    GLShader *shaderForZoom(double zoom);
    void trackTextCaret();
    void trackFocus();

    std::unique_ptr<QTimer> m_configurationTimer;
    double m_zoom = 1.0;
    double m_targetZoom = 1.0;
    double m_sourceZoom = 1.0;
    double m_zoomFactor = 1.25;
    MouseTrackingType m_mouseTracking = MouseTrackingProportional;
    MousePointerType m_mousePointer = MousePointerScale;
    QPoint m_cursorPoint;
    QPoint m_prevPoint;
    QTime m_lastMouseEvent;
    std::unique_ptr<CursorItem> m_cursorItem;
    bool m_cursorHidden = false;
    QTimeLine m_timeline;
    int m_xMove = 0;
    int m_yMove = 0;
    int m_xTranslation = 0;
    int m_yTranslation = 0;
    double m_moveFactor = 20.0;
    AnimationClock m_clock;
    std::map<LogicalOutput *, OffscreenData> m_offscreenData;
    std::unique_ptr<GLShader> m_pixelGridShader;
    double m_pixelGridZoom;
    std::unique_ptr<QAction> m_zoomInAxisAction;
    std::unique_ptr<QAction> m_zoomOutAxisAction;
    Qt::KeyboardModifiers m_axisModifiers;
    std::unique_ptr<QAction> m_touchpadAction;
    double m_lastPinchProgress = 0;

    std::unique_ptr<TextCaretTracker> m_textCaretTracker;
#if KWIN_BUILD_QACCESSIBILITYCLIENT
    std::unique_ptr<FocusTracker> m_focusTracker;
#endif
    std::optional<QPoint> m_focusPoint = std::nullopt;
    QTime m_lastFocusEvent;
    int m_focusDelay = 350; // in milliseconds
};

} // namespace

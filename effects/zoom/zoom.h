/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_ZOOM_H
#define KWIN_ZOOM_H

#include <config-kwin.h>

#include <kwineffects.h>
#include <QTime>
#include <QTimeLine>

namespace KWin
{

#if HAVE_ACCESSIBILITY
class ZoomAccessibilityIntegration;
#endif

class GLTexture;
class XRenderPicture;

class ZoomEffect
    : public Effect
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
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    // for properties
    qreal configuredZoomFactor() const {
        return zoomFactor;
    }
    int configuredMousePointer() const {
        return mousePointer;
    }
    int configuredMouseTracking() const {
        return mouseTracking;
    }
    bool isFocusTrackingEnabled() const;
    bool isTextCaretTrackingEnabled() const;
    int configuredFocusDelay() const {
        return focusDelay;
    }
    qreal configuredMoveFactor() const {
        return moveFactor;
    }
    qreal targetZoom() const {
        return target_zoom;
    }
private Q_SLOTS:
    inline void zoomIn() { zoomIn(-1.0); };
    void zoomIn(double to);
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
    void slotMouseChanged(const QPoint& pos, const QPoint& old,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void recreateTexture();
private:
    void showCursor();
    void hideCursor();
    void moveZoom(int x, int y);
private:
#if HAVE_ACCESSIBILITY
    ZoomAccessibilityIntegration *m_accessibilityIntegration = nullptr;
#endif
    double zoom;
    double target_zoom;
    double source_zoom;
    bool polling; // Mouse polling
    double zoomFactor;
    enum MouseTrackingType { MouseTrackingProportional = 0, MouseTrackingCentred = 1, MouseTrackingPush = 2, MouseTrackingDisabled = 3 };
    MouseTrackingType mouseTracking;
    enum MousePointerType { MousePointerScale = 0, MousePointerKeep = 1, MousePointerHide = 2 };
    MousePointerType mousePointer;
    int focusDelay;
    QPoint cursorPoint;
    QPoint cursorHotSpot;
    QPoint focusPoint;
    QPoint prevPoint;
    QTime lastMouseEvent;
    QTime lastFocusEvent;
    QScopedPointer<GLTexture> texture;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    QScopedPointer<XRenderPicture> xrenderPicture;
#endif
    int imageWidth;
    int imageHeight;
    bool isMouseHidden;
    QTimeLine timeline;
    int xMove, yMove;
    double moveFactor;
};

} // namespace

#endif

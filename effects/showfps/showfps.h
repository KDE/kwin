/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SHOWFPS_H
#define KWIN_SHOWFPS_H

#include <QElapsedTimer>
#include <QFont>

#include <kwineffects.h>


namespace KWin
{
class GLTexture;

class ShowFpsEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal alpha READ configuredAlpha)
    Q_PROPERTY(int x READ configuredX)
    Q_PROPERTY(int y READ configuredY)
    Q_PROPERTY(QRect fpsTextRect READ configuredFpsTextRect)
    Q_PROPERTY(int textAlign READ configuredTextAlign)
    Q_PROPERTY(QFont textFont READ configuredTextFont)
    Q_PROPERTY(QColor textColor READ configuredTextColor)
public:
    ShowFpsEffect();
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintScreen() override;
    enum { INSIDE_GRAPH, NOWHERE, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT }; // fps text position

    // for properties
    qreal configuredAlpha() const {
        return alpha;
    }
    int configuredX() const {
        return x;
    }
    int configuredY() const {
        return y;
    }
    QRect configuredFpsTextRect() const {
        return fpsTextRect;
    }
    int configuredTextAlign() const {
        return textAlign;
    }
    QFont configuredTextFont() const {
        return textFont;
    }
    QColor configuredTextColor() const {
        return textColor;
    }
private:
    void paintGL(int fps, const QMatrix4x4 &projectionMatrix);
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    void paintXrender(int fps);
#endif
    void paintQPainter(int fps);
    void paintFPSGraph(int x, int y);
    void paintDrawSizeGraph(int x, int y);
    void paintGraph(int x, int y, QList<int> values, QList<int> lines, bool colorize);
    QImage fpsTextImage(int fps);
    QElapsedTimer t;
    enum { NUM_PAINTS = 100 }; // remember time needed to paint this many paints
    int paints[ NUM_PAINTS ]; // time needed to paint
    int paint_size[ NUM_PAINTS ]; // number of pixels painted
    int paints_pos;  // position in the queue
    enum { MAX_FPS = 200 };
    qint64 frames[ MAX_FPS ]; // the time when the frame was done
    int frames_pos; // position in the queue
    double alpha;
    int x;
    int y;
    QRect fps_rect;
    QScopedPointer<GLTexture> fpsText;
    int textPosition;
    QFont textFont;
    QColor textColor;
    QRect fpsTextRect;
    int textAlign;
    QScopedPointer<EffectFrame> m_noBenchmark;
};

} // namespace

#endif

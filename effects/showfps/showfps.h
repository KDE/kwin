/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_SHOWFPS_H
#define KWIN_SHOWFPS_H

#include <QTime>
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
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintScreen();
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
    QTime t;
    enum { NUM_PAINTS = 100 }; // remember time needed to paint this many paints
    int paints[ NUM_PAINTS ]; // time needed to paint
    int paint_size[ NUM_PAINTS ]; // number of pixels painted
    int paints_pos;  // position in the queue
    enum { MAX_FPS = 200 };
    int frames[ MAX_FPS ]; // (sec*1000+msec) of the time the frame was done
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

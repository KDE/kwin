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

#include <QtCore/QTime>

#include <kwineffects.h>


namespace KWin
{
class GLTexture;

class ShowFpsEffect
    : public Effect
{
public:
    ShowFpsEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintScreen();
    enum { INSIDE_GRAPH, NOWHERE, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT }; // fps text position
private:
#ifdef KWIN_HAVE_OPENGL
    void paintGL(int fps);
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    void paintXrender(int fps);
#endif
    void paintFPSGraph(int x, int y);
    void paintDrawSizeGraph(int x, int y);
    void paintGraph(int x, int y, QList<int> values, QList<int> lines, bool colorize);
    void paintFPSText(int fps);
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
#ifdef KWIN_HAVE_OPENGL
    GLTexture *fpsText;
#endif
    int textPosition;
    QFont textFont;
    QColor textColor;
    QRect fpsTextRect;
    int textAlign;
};

} // namespace

#endif

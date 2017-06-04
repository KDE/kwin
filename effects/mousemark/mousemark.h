/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_MOUSEMARK_H
#define KWIN_MOUSEMARK_H

#include <kwineffects.h>
#include <kwinglutils.h>
#include <kwinxrenderutils.h>

struct xcb_render_color_t;

namespace KWin
{

class MouseMarkEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int width READ configuredWidth)
    Q_PROPERTY(QColor color READ configuredColor)
public:
    MouseMarkEffect();
    ~MouseMarkEffect();
    void reconfigure(ReconfigureFlags) override;
    void paintScreen(int mask, QRegion region, ScreenPaintData& data) override;
    bool isActive() const override;

    // for properties
    int configuredWidth() const {
        return width;
    }
    QColor configuredColor() const {
        return color;
    }
private Q_SLOTS:
    void clear();
    void clearLast();
    void slotMouseChanged(const QPoint& pos, const QPoint& old,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void screenLockingChanged(bool locked);
private:
    typedef QVector< QPoint > Mark;
    void drawMark(QPainter *painter, const Mark &mark);
    static Mark createArrow(QPoint arrow_start, QPoint arrow_end);
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    void addRect(const QPoint &p1, const QPoint &p2, xcb_rectangle_t *r, xcb_render_color_t *c);
#endif
    QVector< Mark > marks;
    Mark drawing;
    QPoint arrow_start;
    int width;
    QColor color;
};

} // namespace

#endif

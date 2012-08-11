/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

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

#ifndef KWIN_TRACKMOUSE_H
#define KWIN_TRACKMOUSE_H

#include <kwineffects.h>

class KAction;

namespace KWin
{
class GLTexture;

class TrackMouseEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(Qt::KeyboardModifiers modifiers READ modifiers)
    Q_PROPERTY(bool mousePolling READ isMousePolling)
public:
    TrackMouseEffect();
    virtual ~TrackMouseEffect();
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void reconfigure(ReconfigureFlags);
    virtual bool isActive() const;

    // for properties
    Qt::KeyboardModifiers modifiers() const {
        return m_modifiers;
    }
    bool isMousePolling() const {
        return m_mousePolling;
    }
private slots:
    void toggle();
    void slotMouseChanged(const QPoint& pos, const QPoint& old,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
private:
    bool init();
    void loadTexture();
    QRect m_lastRect[2];
    bool m_active, m_mousePolling;
    float m_angle;
    float m_angleBase;
    GLTexture* m_texture[2];
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    QPixmap *m_pixmap[2];
#endif
    KAction* m_action;
    Qt::KeyboardModifiers m_modifiers;
};

} // namespace

#endif

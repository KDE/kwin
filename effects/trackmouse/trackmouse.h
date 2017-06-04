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

class QAction;

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
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, QRegion region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    void reconfigure(ReconfigureFlags) override;
    bool isActive() const override;

    // for properties
    Qt::KeyboardModifiers modifiers() const {
        return m_modifiers;
    }
    bool isMousePolling() const {
        return m_mousePolling;
    }
private Q_SLOTS:
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
    QSize m_size[2];
    XRenderPicture *m_picture[2];
#endif
    QAction* m_action;
    QImage m_image[2];
    Qt::KeyboardModifiers m_modifiers;
};

} // namespace

#endif

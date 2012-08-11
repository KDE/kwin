/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#ifndef KWIN_LOOKINGGLASS_H
#define KWIN_LOOKINGGLASS_H

#include <kwineffects.h>

class KActionCollection;

namespace KWin
{

class GLRenderTarget;
class GLShader;
class GLTexture;
class GLVertexBuffer;

/**
 * Enhanced magnifier
 **/
class LookingGlassEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int initialRadius READ initialRadius)
public:
    LookingGlassEffect();
    virtual ~LookingGlassEffect();

    virtual void reconfigure(ReconfigureFlags);

    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void postPaintScreen();
    virtual bool isActive() const;

    static bool supported();

    // for properties
    int initialRadius() const {
        return initialradius;
    }
public slots:
    void toggle();
    void zoomIn();
    void zoomOut();
    void slotMouseChanged(const QPoint& pos, const QPoint& old,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    bool loadData();
    double zoom;
    double target_zoom;
    bool polling; // Mouse polling
    int radius;
    int initialradius;
    KActionCollection* actionCollection;
    GLTexture *m_texture;
    GLRenderTarget *m_fbo;
    GLVertexBuffer *m_vbo;
    GLShader *m_shader;
    bool m_enabled;
    bool m_valid;
};

} // namespace

#endif

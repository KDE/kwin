/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_MAGNIFIER_H
#define KWIN_MAGNIFIER_H

#include <kwineffects.h>

namespace KWin
{

class GLRenderTarget;
class GLTexture;
class XRenderPicture;

class MagnifierEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(QSize magnifierSize READ magnifierSize)
    Q_PROPERTY(qreal targetZoom READ targetZoom)
public:
    MagnifierEffect();
    ~MagnifierEffect() override;
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    static bool supported();

    // for properties
    QSize magnifierSize() const {
        return magnifier_size;
    }
    qreal targetZoom() const {
        return target_zoom;
    }
private Q_SLOTS:
    void zoomIn();
    void zoomOut();
    void toggle();
    void slotMouseChanged(const QPoint& pos, const QPoint& old,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void destroyPixmap();
private:
    QRect magnifierArea(QPoint pos = cursorPos()) const;
    double zoom;
    double target_zoom;
    bool polling; // Mouse polling
    QSize magnifier_size;
    GLTexture *m_texture;
    GLRenderTarget *m_fbo;
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    xcb_pixmap_t m_pixmap;
    QSize m_pixmapSize;
    QScopedPointer<XRenderPicture> m_picture;
#endif
};

} // namespace

#endif

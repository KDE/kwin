/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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

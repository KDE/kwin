/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_LOOKINGGLASS_H
#define KWIN_LOOKINGGLASS_H

#include <kwineffects.h>

namespace KWin
{

class GLRenderTarget;
class GLShader;
class GLTexture;
class GLVertexBuffer;

/**
 * Enhanced magnifier
 */
class LookingGlassEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int initialRadius READ initialRadius)
public:
    LookingGlassEffect();
    ~LookingGlassEffect() override;

    void reconfigure(ReconfigureFlags) override;

    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    bool isActive() const override;

    static bool supported();

    // for properties
    int initialRadius() const {
        return initialradius;
    }
public Q_SLOTS:
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
    GLTexture *m_texture;
    GLRenderTarget *m_fbo;
    GLVertexBuffer *m_vbo;
    GLShader *m_shader;
    bool m_enabled;
    bool m_valid;
};

} // namespace

#endif

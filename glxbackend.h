/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_GLX_BACKEND_H
#define KWIN_GLX_BACKEND_H
#include "scene_opengl.h"

namespace KWin
{

class FBConfigInfo
{
public:
    GLXFBConfig fbconfig;
    int bind_texture_format;
    int texture_targets;
    int y_inverted;
    int mipmap;
};

/**
 * @brief OpenGL Backend using GLX over an X overlay window.
 **/
class GlxBackend : public OpenGLBackend
{
public:
    GlxBackend();
    virtual ~GlxBackend();
    virtual void screenGeometryChanged(const QSize &size);
    virtual SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture);
    virtual QRegion prepareRenderingFrame();
    virtual void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion);

protected:
    virtual void present();

private:
    void init();
    bool initBuffer();
    bool initDrawableConfigs();
    void waitSync();
    bool initRenderingContext();
    bool initFbConfig();
    void setSwapInterval(int interval);

    Window window;
    FBConfigInfo fbcdrawableinfo[ 32 + 1 ];
    GLXFBConfig fbconfig;
    GLXWindow glxWindow;
    GLXContext ctx;
    int m_bufferAge;
    bool haveSwapInterval, haveWaitSync;
    friend class GlxTexture;
};

/**
 * @brief Texture using an GLXPixmap.
 **/
class GlxTexture : public SceneOpenGL::TexturePrivate
{
public:
    virtual ~GlxTexture();
    virtual void onDamage();
    virtual void findTarget();
    virtual bool loadTexture(const Pixmap& pix, const QSize& size, int depth);
    virtual OpenGLBackend *backend();

private:
    friend class GlxBackend;
    GlxTexture(SceneOpenGL::Texture *texture, GlxBackend *backend);
    SceneOpenGL::Texture *q;
    GlxBackend *m_backend;
    GLXPixmap m_glxpixmap; // the glx pixmap the texture is bound to
};

} // namespace
#endif // KWIN_GLX_BACKEND_H

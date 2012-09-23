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
#ifndef KWIN_EGL_ON_X_BACKEND_H
#define KWIN_EGL_ON_X_BACKEND_H
#include "scene_opengl.h"

namespace KWin
{

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 **/
class EglOnXBackend : public OpenGLBackend
{
public:
    EglOnXBackend();
    virtual ~EglOnXBackend();
    virtual void screenGeometryChanged(const QSize &size);
    virtual SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture);
    virtual void prepareRenderingFrame();
    virtual void endRenderingFrame(int mask, const QRegion &damage);

protected:
    virtual void flushBuffer();

private:
    void init();
    bool initBufferConfigs();
    bool initRenderingContext();
    EGLDisplay dpy;
    EGLConfig config;
    EGLSurface surface;
    EGLContext ctx;
    int surfaceHasSubPost;
    friend class EglTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 **/
class EglTexture : public SceneOpenGL::TexturePrivate
{
public:
    virtual ~EglTexture();
    virtual void onDamage();
    virtual void findTarget();
    virtual bool loadTexture(const Pixmap& pix, const QSize& size, int depth);
    virtual OpenGLBackend *backend();

private:
    friend class EglOnXBackend;
    EglTexture(SceneOpenGL::Texture *texture, EglOnXBackend *backend);
    SceneOpenGL::Texture *q;
    EglOnXBackend *m_backend;
    EGLImageKHR m_image;
};

} // namespace

#endif //  KWIN_EGL_ON_X_BACKEND_H

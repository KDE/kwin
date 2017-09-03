/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_backend.h"
#include "scene_opengl.h"

namespace KWin
{
class VirtualBackend;
class GLTexture;
class GLRenderTarget;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 **/
class EglGbmBackend : public AbstractEglBackend
{
public:
    EglGbmBackend(VirtualBackend *b);
    virtual ~EglGbmBackend();
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool usesOverlayWindow() const override;
    void init() override;

protected:
    void present() override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    void initGbmDevice();
    VirtualBackend *m_backend;
    GLTexture *m_backBuffer = nullptr;
    GLRenderTarget *m_fbo = nullptr;
    int m_frameCounter = 0;
    friend class EglGbmTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 **/
class EglGbmTexture : public AbstractEglTexture
{
public:
    virtual ~EglGbmTexture();

private:
    friend class EglGbmBackend;
    EglGbmTexture(SceneOpenGL::Texture *texture, EglGbmBackend *backend);
};

} // namespace

#endif

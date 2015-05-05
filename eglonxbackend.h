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
#include "abstract_egl_backend.h"
#include "scene_opengl.h"

namespace KWin
{

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 **/
class KWIN_EXPORT EglOnXBackend : public AbstractEglBackend
{
public:
    EglOnXBackend();
    explicit EglOnXBackend(xcb_connection_t *connection, Display *display, xcb_window_t rootWindow, int screenNumber, xcb_window_t renderingWindow);
    virtual ~EglOnXBackend();
    virtual void screenGeometryChanged(const QSize &size);
    virtual SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture);
    virtual QRegion prepareRenderingFrame();
    virtual void endRenderingFrame(const QRegion &damage, const QRegion &damagedRegion);
    virtual OverlayWindow* overlayWindow() override;
    virtual bool usesOverlayWindow() const override;

protected:
    virtual void present();

private:
    void init();
    bool initBufferConfigs();
    bool initRenderingContext();
    /**
     * @brief The OverlayWindow used by this Backend.
     **/
    OverlayWindow *m_overlayWindow;
    int surfaceHasSubPost;
    int m_bufferAge;
    bool m_usesOverlayWindow;
    xcb_connection_t *m_connection;
    Display *m_x11Display;
    xcb_window_t m_rootWindow;
    int m_x11ScreenNumber;
    xcb_window_t m_renderingWindow = XCB_WINDOW_NONE;
    friend class EglTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 **/
class EglTexture : public AbstractEglTexture
{
public:
    virtual ~EglTexture();
    virtual void onDamage();

private:
    friend class EglOnXBackend;
    EglTexture(SceneOpenGL::Texture *texture, EglOnXBackend *backend);
};

} // namespace

#endif //  KWIN_EGL_ON_X_BACKEND_H

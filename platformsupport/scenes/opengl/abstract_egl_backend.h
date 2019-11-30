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
#ifndef KWIN_ABSTRACT_EGL_BACKEND_H
#define KWIN_ABSTRACT_EGL_BACKEND_H
#include "backend.h"
#include "texture.h"

#include <QObject>
#include <epoxy/egl.h>
#include <fixx11h.h>

class QOpenGLFramebufferObject;

namespace KWayland
{
namespace Server
{
class BufferInterface;
}
}

namespace KWin
{

class EglDmabuf;

class KWIN_EXPORT AbstractEglBackend : public QObject, public OpenGLBackend
{
    Q_OBJECT
public:
    ~AbstractEglBackend() override;
    bool makeCurrent() override;
    void doneCurrent() override;

    EGLDisplay eglDisplay() const {
        return m_display;
    }
    EGLContext context() const {
        return m_context;
    }
    EGLSurface surface() const {
        return m_surface;
    }
    EGLConfig config() const {
        return m_config;
    }

protected:
    AbstractEglBackend();
    void setEglDisplay(const EGLDisplay &display);
    void setSurface(const EGLSurface &surface);
    void setConfig(const EGLConfig &config);
    void cleanup();
    virtual void cleanupSurfaces();
    bool initEglAPI();
    void initKWinGL();
    void initBufferAge();
    void initClientExtensions();
    void initWayland();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;

    bool createContext();

private:
    void unbindWaylandDisplay();

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLConfig m_config = nullptr;
    QList<QByteArray> m_clientExtensions;
    EglDmabuf *m_dmaBuf = nullptr;
};

class KWIN_EXPORT AbstractEglTexture : public SceneOpenGLTexturePrivate
{
public:
    ~AbstractEglTexture() override;
    bool loadTexture(WindowPixmap *pixmap) override;
    void updateTexture(WindowPixmap *pixmap) override;
    OpenGLBackend *backend() override;

protected:
    AbstractEglTexture(SceneOpenGLTexture *texture, AbstractEglBackend *backend);
    EGLImageKHR image() const {
        return m_image;
    }
    void setImage(const EGLImageKHR &img) {
        m_image = img;
    }
    SceneOpenGLTexture *texture() const {
        return q;
    }

private:
    bool loadShmTexture(const QPointer<KWayland::Server::BufferInterface> &buffer);
    bool loadEglTexture(const QPointer<KWayland::Server::BufferInterface> &buffer);
    bool loadDmabufTexture(const QPointer< KWayland::Server::BufferInterface > &buffer);
    bool loadInternalImageObject(WindowPixmap *pixmap);
    EGLImageKHR attach(const QPointer<KWayland::Server::BufferInterface> &buffer);
    bool updateFromFBO(const QSharedPointer<QOpenGLFramebufferObject> &fbo);
    bool updateFromInternalImageObject(WindowPixmap *pixmap);
    SceneOpenGLTexture *q;
    AbstractEglBackend *m_backend;
    EGLImageKHR m_image;
};

}

#endif

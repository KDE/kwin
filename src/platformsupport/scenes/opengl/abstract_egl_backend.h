/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_ABSTRACT_EGL_BACKEND_H
#define KWIN_ABSTRACT_EGL_BACKEND_H
#include "openglbackend.h"
#include "texture.h"

#include <KWaylandServer/clientbufferref.h>

#include <QObject>
#include <epoxy/egl.h>

class QOpenGLFramebufferObject;

namespace KWin
{

class AbstractOutput;

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

    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *output) const override;

    static void setPrimaryBackend(AbstractEglBackend *primaryBackend) {
        s_primaryBackend = primaryBackend;
    }
    static AbstractEglBackend *primaryBackend() {
        return s_primaryBackend;
    }

    bool isPrimary() const {
        return this == s_primaryBackend;
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
    void finishWayland();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;
    bool createContext();

private:
    void teardown();

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLConfig m_config = nullptr;
    QList<QByteArray> m_clientExtensions;

    static AbstractEglBackend * s_primaryBackend;
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

    SceneOpenGLTexture *texture() const {
        return q;
    }

private:
    void createTextureSubImage(const QImage &image, const QRegion &damage);
    bool createTextureImage(const QImage &image);
    bool loadInternalImageObject(WindowPixmap *pixmap);
    bool updateFromFBO(const QSharedPointer<QOpenGLFramebufferObject> &fbo);
    bool updateFromInternalImageObject(WindowPixmap *pixmap);
    SceneOpenGLTexture *q;
    AbstractEglBackend *m_backend;
};

}

#endif

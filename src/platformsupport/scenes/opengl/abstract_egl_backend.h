/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "openglbackend.h"

#include <QObject>
#include <epoxy/egl.h>

struct wl_display;
struct wl_resource;

namespace KWin
{

struct DmaBufAttributes;
class EglDmabuf;
class Output;

class KWIN_EXPORT AbstractEglBackend : public OpenGLBackend
{
    Q_OBJECT
public:
    ~AbstractEglBackend() override;
    bool makeCurrent() override;
    void doneCurrent() override;

    EGLDisplay eglDisplay() const
    {
        return m_display;
    }
    EGLContext context() const
    {
        return m_context;
    }
    EGLSurface surface() const
    {
        return m_surface;
    }
    EGLConfig config() const
    {
        return m_config;
    }

    std::shared_ptr<GLTexture> textureForOutput(Output *output) const override;
    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const override;

    dev_t deviceId() const;
    virtual bool prefer10bpc() const;
    EglDmabuf *dmabuf() const;

    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes) const;

protected:
    AbstractEglBackend(dev_t deviceId = 0);
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
    EGLContext ensureGlobalShareContext();
    void destroyGlobalShareContext();
    EGLContext createContextInternal(EGLContext sharedContext);

    void teardown();

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLConfig m_config = nullptr;
    // note: m_dmaBuf is nullptr if this is not the primary backend
    EglDmabuf *m_dmaBuf = nullptr;
    QList<QByteArray> m_clientExtensions;
    const dev_t m_deviceId;

    static AbstractEglBackend *s_primaryBackend;
};

}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "platformsupport/scenes/opengl/egldisplay.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "wayland/linuxdmabufv1clientbuffer.h"

#include <QObject>
#include <epoxy/egl.h>

struct wl_display;
struct wl_resource;

namespace KWin
{

typedef GLboolean (*eglBindWaylandDisplayWL_func)(::EGLDisplay dpy, wl_display *display);
typedef GLboolean (*eglUnbindWaylandDisplayWL_func)(::EGLDisplay dpy, wl_display *display);
typedef GLboolean (*eglQueryWaylandBufferWL_func)(::EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);

struct AbstractEglBackendFunctions
{
    eglBindWaylandDisplayWL_func eglBindWaylandDisplayWL = nullptr;
    eglUnbindWaylandDisplayWL_func eglUnbindWaylandDisplayWL = nullptr;
    eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL = nullptr;
};

struct DmaBufAttributes;
class Output;

class KWIN_EXPORT AbstractEglBackend : public OpenGLBackend
{
    Q_OBJECT
public:
    ~AbstractEglBackend() override;
    bool makeCurrent() override;
    void doneCurrent() override;

    const AbstractEglBackendFunctions *functions() const
    {
        return &m_functions;
    }
    ::EGLDisplay eglDisplay() const;
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
    EglDisplay *eglDisplayObject() const;

    bool testImportBuffer(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer) override;
    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const override;

    QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> tranches() const;
    dev_t deviceId() const;
    virtual bool prefer10bpc() const;

    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes) const;
    EGLImageKHR importBufferAsImage(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer);

protected:
    AbstractEglBackend(dev_t deviceId = 0);
    void setSurface(const EGLSurface &surface);
    void setConfig(const EGLConfig &config);
    void cleanup();
    virtual void cleanupSurfaces();
    void setEglDisplay(EglDisplay *display);
    void initKWinGL();
    void initClientExtensions();
    void initWayland();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;
    bool createContext();
    virtual bool initBufferConfigs();

private:
    EGLContext ensureGlobalShareContext();
    void destroyGlobalShareContext();
    EGLContext createContextInternal(EGLContext sharedContext);

    void teardown();

    AbstractEglBackendFunctions m_functions;
    EglDisplay *m_display = nullptr;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLConfig m_config = nullptr;
    QList<QByteArray> m_clientExtensions;
    const dev_t m_deviceId;
    QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> m_tranches;
    QHash<uint32_t, QVector<uint64_t>> m_supportedFormats;
    QHash<KWaylandServer::LinuxDmaBufV1ClientBuffer *, EGLImageKHR> m_importedBuffers;

    static AbstractEglBackend *s_primaryBackend;
};

}

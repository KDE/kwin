/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "platformsupport/scenes/opengl/eglcontext.h"
#include "platformsupport/scenes/opengl/egldisplay.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "wayland/linuxdmabufv1clientbuffer.h"

#include <QObject>
#include <epoxy/egl.h>

struct wl_display;
struct wl_resource;

namespace KWin
{

struct DmaBufAttributes;
class Output;

class KWIN_EXPORT AbstractEglBackend : public OpenGLBackend
{
    Q_OBJECT
public:
    ~AbstractEglBackend() override;
    bool makeCurrent() override;
    void doneCurrent() override;

    EGLSurface surface() const;
    EGLConfig config() const;
    EglDisplay *eglDisplayObject() const;
    EglContext *contextObject();

    bool testImportBuffer(GraphicsBuffer *buffer) override;
    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const override;

    QVector<LinuxDmaBufV1Feedback::Tranche> tranches() const;
    dev_t deviceId() const;
    virtual bool prefer10bpc() const;

    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;
    EGLImageKHR importDmaBufAsImage(const DmaBufAttributes &attributes) const;
    EGLImageKHR importBufferAsImage(GraphicsBuffer *buffer);

protected:
    AbstractEglBackend(dev_t deviceId = 0);
    void setSurface(const EGLSurface &surface);
    void cleanup();
    virtual void cleanupSurfaces();
    void setEglDisplay(EglDisplay *display);
    void initKWinGL();
    void initClientExtensions();
    void initWayland();
    bool hasClientExtension(const QByteArray &ext) const;
    bool isOpenGLES() const;
    bool createContext(EGLConfig config);

private:
    bool ensureGlobalShareContext(EGLConfig config);
    void destroyGlobalShareContext();
    ::EGLContext createContextInternal(::EGLContext sharedContext);

    void teardown();

    EglDisplay *m_display = nullptr;
    EGLSurface m_surface = EGL_NO_SURFACE;
    std::unique_ptr<EglContext> m_context;
    QList<QByteArray> m_clientExtensions;
    const dev_t m_deviceId;
    QVector<LinuxDmaBufV1Feedback::Tranche> m_tranches;
    QHash<GraphicsBuffer *, EGLImageKHR> m_importedBuffers;
};

}

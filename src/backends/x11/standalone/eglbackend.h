/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "eglonxbackend.h"
#include "openglsurfacetexture_x11.h"
#include "outputlayer.h"
#include "utils/damagejournal.h"

#include <kwingltexture.h>
#include <kwingltexture_p.h>

namespace KWin
{

class EglPixmapTexturePrivate;
class SoftwareVsyncMonitor;
class X11StandalonePlatform;
class EglBackend;

class EglLayer : public OutputLayer
{
public:
    EglLayer(EglBackend *backend);

    OutputLayerBeginFrameInfo beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    EglBackend *const m_backend;
};

class EglBackend : public EglOnXBackend
{
    Q_OBJECT

public:
    EglBackend(Display *display, X11StandalonePlatform *platform);
    ~EglBackend() override;

    void init() override;

    SurfaceTexture *createSurfaceTextureX11(SurfacePixmapX11 *texture) override;
    OutputLayerBeginFrameInfo beginFrame();
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion);
    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;

private:
    void screenGeometryChanged();
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);
    void vblank(std::chrono::nanoseconds timestamp);

    X11StandalonePlatform *m_backend;
    SoftwareVsyncMonitor *m_vsyncMonitor;
    DamageJournal m_damageJournal;
    QScopedPointer<GLFramebuffer> m_fbo;
    int m_bufferAge = 0;
    QRegion m_lastRenderedRegion;
    QScopedPointer<EglLayer> m_layer;
};

class EglPixmapTexture : public GLTexture
{
public:
    explicit EglPixmapTexture(EglBackend *backend);

    bool create(SurfacePixmapX11 *texture);

private:
    Q_DECLARE_PRIVATE(EglPixmapTexture)
};

class EglPixmapTexturePrivate : public GLTexturePrivate
{
public:
    EglPixmapTexturePrivate(EglPixmapTexture *texture, EglBackend *backend);
    ~EglPixmapTexturePrivate() override;

    bool create(SurfacePixmapX11 *texture);

protected:
    void onDamage() override;

private:
    EglPixmapTexture *q;
    EglBackend *m_backend;
    EGLImageKHR m_image = EGL_NO_IMAGE_KHR;
};

class EglSurfaceTextureX11 : public OpenGLSurfaceTextureX11
{
public:
    EglSurfaceTextureX11(EglBackend *backend, SurfacePixmapX11 *texture);

    bool create() override;
    void update(const QRegion &region) override;
};

} // namespace KWin

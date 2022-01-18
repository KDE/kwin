/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "eglonxbackend.h"
#include "openglsurfacetexture_x11.h"
#include "utils/common.h"

#include <kwingltexture.h>
#include <kwingltexture_p.h>

namespace KWin
{

class EglPixmapTexturePrivate;
class SoftwareVsyncMonitor;
class X11StandalonePlatform;

class EglBackend : public EglOnXBackend
{
    Q_OBJECT

public:
    EglBackend(Display *display, X11StandalonePlatform *platform);
    ~EglBackend() override;

    void init() override;

    SurfaceTexture *createSurfaceTextureX11(SurfacePixmapX11 *texture) override;
    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    void screenGeometryChanged();
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);
    void vblank(std::chrono::nanoseconds timestamp);

    X11StandalonePlatform *m_backend;
    SoftwareVsyncMonitor *m_vsyncMonitor;
    DamageJournal m_damageJournal;
    int m_bufferAge = 0;
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

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_backend.h"
#include "utils/common.h"

#include <kwinglutils.h>

#include <QPointer>
#include <QSharedPointer>
#include <optional>

struct gbm_surface;
struct gbm_bo;

namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{
class AbstractOutput;
class DrmAbstractOutput;
class DrmBuffer;
class DrmGbmBuffer;
class DrmOutput;
class GbmSurface;
class GbmBuffer;
class DumbSwapchain;
class ShadowBuffer;
class DrmBackend;
class DrmGpu;
class EglGbmLayer;

struct GbmFormat {
    uint32_t drmFormat = 0;
    EGLint redSize = -1;
    EGLint greenSize = -1;
    EGLint blueSize = -1;
    EGLint alphaSize = -1;
};
bool operator==(const GbmFormat &lhs, const GbmFormat &rhs);

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend);
    ~EglGbmBackend() override;

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void init() override;
    bool scanout(AbstractOutput *output, SurfaceItem *surfaceItem) override;
    bool prefer10bpc() const override;

    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    bool directScanoutAllowed(AbstractOutput *output) const override;

    QSharedPointer<DrmBuffer> testBuffer(DrmAbstractOutput *output);
    EGLConfig config(uint32_t format) const;
    GbmFormat gbmFormatForDrmFormat(uint32_t format) const;
    std::optional<uint32_t> chooseFormat(DrmAbstractOutput *output) const;

protected:
    void cleanupSurfaces() override;
    void aboutToStartPainting(AbstractOutput *output, const QRegion &damage) override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    void addOutput(AbstractOutput *output);
    void removeOutput(AbstractOutput *output);

    QMap<AbstractOutput *, QSharedPointer<EglGbmLayer>> m_surfaces;
    DrmBackend *m_backend;
    QVector<GbmFormat> m_formats;
    QMap<uint32_t, EGLConfig> m_configs;

    friend class EglGbmTexture;
};

} // namespace

#endif

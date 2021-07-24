/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_drm_backend.h"
#include "utils.h"

#include <kwinglutils.h>

#include <QSharedPointer>

struct gbm_surface;
struct gbm_bo;

namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{
class AbstractOutput;
class DrmBuffer;
class DrmGbmBuffer;
class DrmOutput;
class GbmSurface;
class GbmBuffer;
class DumbSwapchain;
class ShadowBuffer;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglDrmBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend, DrmGpu *gpu);
    ~EglGbmBackend() override;

    PlatformSurfaceTexture *createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    PlatformSurfaceTexture *createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    void init() override;
    bool scanout(int screenId, SurfaceItem *surfaceItem) override;

    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    int screenCount() const override {
        return m_outputs.count();
    }

    bool addOutput(DrmOutput *output) override;
    void removeOutput(DrmOutput *output) override;
    bool swapBuffers(DrmOutput *output, const QRegion &dirty) override;
    bool exportFramebuffer(DrmOutput *output, void *data, const QSize &size, uint32_t stride) override;
    int exportFramebufferAsDmabuf(DrmOutput *output, uint32_t *format, uint32_t *stride) override;
    QRegion beginFrameForSecondaryGpu(DrmOutput *output) override;

    bool directScanoutAllowed(int screen) const override;

    QSharedPointer<DrmBuffer> renderTestFrame(DrmOutput *output) override;

protected:
    void cleanupSurfaces() override;
    void aboutToStartPainting(int screenId, const QRegion &damage) override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    enum class ImportMode {
        Dmabuf,
        DumbBuffer
    };
    struct Output {
        DrmOutput *output = nullptr;
        struct RenderData {
            QSharedPointer<ShadowBuffer> shadowBuffer;
            QSharedPointer<GbmSurface> gbmSurface;
            int bufferAge = 0;
            DamageJournal damageJournal;

            // for secondary GPU import
            ImportMode importMode = ImportMode::Dmabuf;
            QSharedPointer<DumbSwapchain> importSwapchain;
        } old, current;

        KWaylandServer::SurfaceInterface *surfaceInterface = nullptr;
    };

    bool doesRenderFit(DrmOutput *output, const Output::RenderData &render);
    bool resetOutput(Output &output, DrmOutput *drmOutput);

    bool makeContextCurrent(const Output::RenderData &output) const;
    void setViewport(const Output &output) const;

    void renderFramebufferToSurface(Output &output);
    QRegion prepareRenderingForOutput(Output &output);
    QSharedPointer<DrmBuffer> importFramebuffer(Output &output, const QRegion &dirty) const;
    QSharedPointer<DrmBuffer> endFrameWithBuffer(int screenId, const QRegion &dirty);
    void updateBufferAge(Output &output, const QRegion &dirty);

    void cleanupRenderData(Output::RenderData &output);

    QVector<Output> m_outputs;
    QVector<Output> m_secondaryGpuOutputs;

    friend class EglGbmTexture;
};

} // namespace

#endif

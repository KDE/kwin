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

struct GbmFormat {
    uint32_t drmFormat;
    EGLint redSize;
    EGLint greenSize;
    EGLint blueSize;
    EGLint alphaSize;
};
bool operator==(const GbmFormat &lhs, const GbmFormat &rhs);

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 */
class EglGbmBackend : public AbstractEglBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *drmBackend, DrmGpu *gpu);
    ~EglGbmBackend() override;

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void init() override;
    bool scanout(AbstractOutput *output, SurfaceItem *surfaceItem) override;
    bool prefer10bpc() const override;

    QSharedPointer<GLTexture> textureForOutput(AbstractOutput *requestedOutput) const override;

    bool hasOutput(AbstractOutput *output) const;
    bool swapBuffers(DrmAbstractOutput *output, const QRegion &dirty);
    bool exportFramebuffer(DrmAbstractOutput *output, void *data, const QSize &size, uint32_t stride);
    bool exportFramebufferAsDmabuf(DrmAbstractOutput *output, int *fds, int *strides, int *offsets, uint32_t *num_fds, uint32_t *format, uint64_t *modifier);

    bool directScanoutAllowed(AbstractOutput *output) const override;

    QSharedPointer<DrmBuffer> renderTestFrame(DrmAbstractOutput *output);
    uint32_t drmFormat(DrmAbstractOutput *output) const;
    DrmGpu *gpu() const;

protected:
    void cleanupSurfaces() override;
    void aboutToStartPainting(AbstractOutput *output, const QRegion &damage) override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    enum class ImportMode {
        Dmabuf,
        DumbBuffer
    };
    struct Output {
        DrmAbstractOutput *output = nullptr;
        bool forceXrgb8888 = false;
        struct RenderData {
            QSharedPointer<ShadowBuffer> shadowBuffer;
            QSharedPointer<GbmSurface> gbmSurface;
            int bufferAge = 0;
            DamageJournal damageJournal;
            GbmFormat format;

            // for secondary GPU import
            ImportMode importMode = ImportMode::Dmabuf;
            QSharedPointer<DumbSwapchain> importSwapchain;
        } old, current;

        KWaylandServer::SurfaceInterface *scanoutSurface = nullptr;
        struct {
            QPointer<KWaylandServer::SurfaceInterface> surface;
            QMap<uint32_t, QVector<uint64_t>> attemptedFormats;
        } scanoutCandidate;
        QSharedPointer<DrmBuffer> scanoutBuffer;
        QPointer<KWaylandServer::SurfaceInterface> oldScanoutCandidate;
    };

    bool doesRenderFit(const Output &output, const Output::RenderData &render);
    bool resetOutput(Output &output);
    bool addOutput(AbstractOutput *output);
    void removeOutput(AbstractOutput *output);

    bool makeContextCurrent(const Output::RenderData &output) const;
    void setViewport(const Output &output) const;

    void renderFramebufferToSurface(Output &output);
    QRegion prepareRenderingForOutput(Output &output);
    QSharedPointer<DrmBuffer> importFramebuffer(Output &output, const QRegion &dirty) const;
    QSharedPointer<DrmBuffer> endFrameWithBuffer(AbstractOutput *output, const QRegion &dirty);
    void updateBufferAge(Output &output, const QRegion &dirty);
    std::optional<GbmFormat> chooseFormat(Output &output) const;

    void cleanupRenderData(Output::RenderData &output);

    QMap<AbstractOutput *, Output> m_outputs;
    DrmBackend *m_backend;
    DrmGpu *m_gpu;
    QVector<GbmFormat> m_formats;
    QMap<uint32_t, EGLConfig> m_configs;

    static EglGbmBackend *renderingBackend();

    void setForceXrgb8888(DrmAbstractOutput *output);

    friend class EglGbmTexture;
};

} // namespace

#endif

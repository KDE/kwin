/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_layer.h"

#include <QSharedPointer>
#include <QPointer>
#include <QMap>
#include <QRegion>
#include <optional>
#include <epoxy/egl.h>

namespace KWaylandServer
{
class SurfaceInterface;
class LinuxDmaBufV1ClientBuffer;
}

namespace KWin
{

class GbmSurface;
class DumbSwapchain;
class ShadowBuffer;
class EglGbmBackend;

class EglGbmLayer : public DrmPipelineLayer
{
public:
    EglGbmLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline);
    ~EglGbmLayer();

    std::optional<QRegion> startRendering() override;
    void aboutToStartPainting(const QRegion &damagedRegion) override;
    bool endRendering(const QRegion &damagedRegion) override;
    bool scanout(SurfaceItem *surfaceItem) override;
    QSharedPointer<DrmBuffer> testBuffer() override;
    QSharedPointer<DrmBuffer> currentBuffer() const override;
    bool hasDirectScanoutBuffer() const override;
    QRegion currentDamage() const override;
    QSharedPointer<GLTexture> texture() const override;

private:
    bool createGbmSurface(uint32_t format, const QVector<uint64_t> &modifiers);
    bool createGbmSurface();
    bool doesGbmSurfaceFit(GbmSurface *surf) const;
    bool doesShadowBufferFit(ShadowBuffer *buffer) const;
    bool doesSwapchainFit(DumbSwapchain *swapchain) const;
    void sendDmabufFeedback(KWaylandServer::LinuxDmaBufV1ClientBuffer *failedBuffer);
    bool renderTestBuffer();
    void destroyResources();

    QSharedPointer<DrmBuffer> importBuffer();
    QSharedPointer<DrmBuffer> importDmabuf();
    QSharedPointer<DrmBuffer> importWithCpu();

    enum class MultiGpuImportMode {
        Dmabuf,
        DumbBuffer,
        DumbBufferXrgb8888,
        Failed
    };
    MultiGpuImportMode m_importMode = MultiGpuImportMode::Dmabuf;

    struct {
        QPointer<KWaylandServer::SurfaceInterface> surface;
        QMap<uint32_t, QVector<uint64_t>> attemptedFormats;
        bool attemptedThisFrame = false;
    } m_scanoutCandidate;

    QSharedPointer<DrmBuffer> m_scanoutBuffer;
    QSharedPointer<DrmBuffer> m_currentBuffer;
    QRegion m_currentDamage;
    QSharedPointer<GbmSurface> m_gbmSurface;
    QSharedPointer<GbmSurface> m_oldGbmSurface;
    QSharedPointer<ShadowBuffer> m_shadowBuffer;
    QSharedPointer<ShadowBuffer> m_oldShadowBuffer;
    QSharedPointer<DumbSwapchain> m_importSwapchain;
    QSharedPointer<DumbSwapchain> m_oldImportSwapchain;

    EglGbmBackend *const m_eglBackend;
};

}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"
#include "drm_plane.h"
#include "utils/damagejournal.h"

#include <QMap>
#include <QPointer>
#include <epoxy/egl.h>
#include <optional>

namespace KWin
{

class DrmPipeline;
class MultiGpuSwapchain;
class RenderBackend;
class Swapchain;
class SwapchainSlot;

class DrmPipelineLayer : public OutputLayer
{
    Q_OBJECT

public:
    enum class BufferTarget {
        Normal,
        Dumb,
    };

    explicit DrmPipelineLayer(RenderBackend *backend, DrmPlane *plane);
    explicit DrmPipelineLayer(RenderBackend *backend, DrmGpu *gpu, DrmPlane::TypeIndex type);
    ~DrmPipelineLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame) override;
    bool endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame) override;
    bool preparePresentationTest() override;
    void releaseBuffers() override;

    FormatModifierMap supportedDrmFormats() const override;
    QList<QSize> recommendedSizes() const override;
    FormatModifierMap supportedAsyncDrmFormats() const override;

    std::shared_ptr<DrmFramebuffer> currentBuffer() const;
    DrmPlane *plane() const;

private:
    bool earlyScanoutChecks() override;
    bool importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame) override;
    SurfaceItem *presentedDirectScanoutItem() const override;

    DrmPipeline *pipeline() const;
    // DrmGpu *gpu() const;
    DrmOutput *drmOutput() const;

    void destroyResources();

    enum class MultiGpuImportMode {
        None,
        GpuCopy,
    };
    struct Surface
    {
        bool needsRecreation = false;

        std::shared_ptr<Swapchain> swapchain;
        std::shared_ptr<SwapchainSlot> currentSlot;
        DamageJournal damageJournal;
        std::unique_ptr<MultiGpuSwapchain> importSwapchain;
        MultiGpuImportMode importMode;
        std::shared_ptr<DrmFramebuffer> currentFramebuffer;
        BufferTarget bufferTarget;
        uint32_t requiredAlphaBits = 0;
        OutputTransform transformation = OutputTransform::Kind::Normal;

        BackendOutput::ColorPowerTradeoff tradeoff = BackendOutput::ColorPowerTradeoff::PreferEfficiency;
    };
    bool checkSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits);
    bool doesSurfaceFit(Surface *surface, const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const;
    std::unique_ptr<Surface> createSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const;
    std::unique_ptr<Surface> createSurface(const QSize &size, uint32_t format, const ModifierList &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const;

    std::shared_ptr<DrmFramebuffer> renderTestBuffer(Surface *surface) const;
    std::shared_ptr<DrmFramebuffer> importBuffer(Surface *surface, SwapchainSlot *source, FileDescriptor &&readFence, OutputFrame *frame, const Region &damagedDeviceRegion) const;

    RenderBackend *const m_backend;
    DrmGpu *const m_gpu;
    DrmPlane *const m_plane = nullptr;
    const BufferTarget m_requestedBufferTarget;

    std::unique_ptr<Surface> m_surface;
    std::unique_ptr<Surface> m_oldSurface;
    std::shared_ptr<DrmFramebuffer> m_scanoutBuffer;
};

}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QMap>
#include <QPointer>
#include <chrono>
#include <optional>

#include "core/outputlayer.h"
#include "drm_plane.h"
#include "opengl/gltexture.h"
#include "utils/damagejournal.h"
#include "utils/filedescriptor.h"

namespace KWin
{

class DrmFramebuffer;
class Swapchain;
class SwapchainSlot;
class QPainterSwapchain;
class EglContext;
class EglGbmBackend;
class GraphicsBuffer;
class SurfaceItem;
class GLTexture;
class GLRenderTimeQuery;
class ColorTransformation;
class GlLookUpTable;
class IccProfile;
class IccShader;
class MultiGpuSwapchain;
class RenderDevice;

class EglGbmLayerSurface : public QObject
{
    Q_OBJECT

public:
    enum class BufferTarget {
        Normal,
        Dumb,
    };
    explicit EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend, BufferTarget target = BufferTarget::Normal);
    ~EglGbmLayerSurface();

    std::optional<OutputLayerBeginFrameInfo> startRendering(const QSize &bufferSize, OutputTransform transformation, const FormatModifierMap &formats,
                                                            const std::shared_ptr<ColorDescription> &layerBlendingColor,
                                                            BackendOutput::ColorPowerTradeoff tradeoff,
                                                            uint32_t requiredAlphaBits);
    bool endRendering(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame);

    void destroyResources();
    EglGbmBackend *eglBackend() const;
    std::shared_ptr<DrmFramebuffer> renderTestBuffer(const QSize &bufferSize, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits);
    void forgetDamage();

    std::shared_ptr<DrmFramebuffer> currentBuffer() const;

private:
    enum class MultiGpuImportMode {
        None,
        GpuCopy,
    };
    struct Surface
    {
        ~Surface();

        bool needsRecreation = false;

        std::shared_ptr<EglContext> context;
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

        std::unique_ptr<GLRenderTimeQuery> compositingTimeQuery;
    };
    bool checkSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits);
    bool doesSurfaceFit(Surface *surface, const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const;
    std::unique_ptr<Surface> createSurface(const QSize &size, const FormatModifierMap &formats, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const;
    std::unique_ptr<Surface> createSurface(const QSize &size, uint32_t format, const ModifierList &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget, BackendOutput::ColorPowerTradeoff tradeoff, uint32_t requiredAlphaBits) const;

    std::shared_ptr<DrmFramebuffer> doRenderTestBuffer(Surface *surface) const;
    std::shared_ptr<DrmFramebuffer> importBuffer(Surface *surface, SwapchainSlot *source, FileDescriptor &&readFence, OutputFrame *frame, const Region &damagedDeviceRegion) const;

    std::unique_ptr<Surface> m_surface;
    std::unique_ptr<Surface> m_oldSurface;

    DrmGpu *const m_gpu;
    EglGbmBackend *const m_eglBackend;
    const BufferTarget m_requestedBufferTarget;
};

}

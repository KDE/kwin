/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "multigpuswapchain.h"
#include "core/drmdevice.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "core/renderbackend.h"
#include "core/syncobjtimeline.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "utils/envvar.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_logging.h"
#include "vulkan/vulkan_render_time_query.h"
#include "vulkan/vulkan_swapchain.h"
#include "vulkan/vulkan_texture.h"

namespace KWin
{

class MultiGpuCopy : public QObject
{
    Q_OBJECT

public:
    explicit MultiGpuCopy(RenderDevice *device);
    virtual ~MultiGpuCopy() = default;

    virtual std::optional<MultiGpuSwapchain::Ret> copy(GraphicsBuffer *buffer,
                                                       const Region &damage,
                                                       FileDescriptor &&sync,
                                                       OutputFrame *frame,
                                                       const std::shared_ptr<SyncReleasePoint> &releasePoint) = 0;
    virtual void resetDamageTracking() = 0;
    virtual QSize size() const = 0;
    virtual uint32_t format() const = 0;
    virtual uint64_t modifier() const = 0;

Q_SIGNALS:
    void gpuReset();

public:
    RenderDevice *m_device = nullptr;
    DamageJournal m_journal;
};

class VulkanMultiGpuCopy : public MultiGpuCopy
{
    Q_OBJECT

public:
    explicit VulkanMultiGpuCopy(RenderDevice *device, std::unique_ptr<VulkanSwapchain> &&swapchain);

    std::optional<MultiGpuSwapchain::Ret> copy(GraphicsBuffer *buffer,
                                               const Region &damage,
                                               FileDescriptor &&sync,
                                               OutputFrame *frame,
                                               const std::shared_ptr<SyncReleasePoint> &releasePoint) override;
    void resetDamageTracking() override;
    QSize size() const override;
    uint32_t format() const override;
    uint64_t modifier() const override;

    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::shared_ptr<VulkanSwapchainSlot> m_currentSlot;
};

class EglMultiGpuCopy : public MultiGpuCopy
{
    Q_OBJECT

public:
    explicit EglMultiGpuCopy(RenderDevice *device,
                             std::shared_ptr<EglContext> &&context,
                             std::shared_ptr<EglSwapchain> &&swapchain);
    ~EglMultiGpuCopy() override;

    std::optional<MultiGpuSwapchain::Ret> copy(GraphicsBuffer *buffer,
                                               const Region &damage,
                                               FileDescriptor &&sync,
                                               OutputFrame *frame,
                                               const std::shared_ptr<SyncReleasePoint> &releasePoint) override;
    void resetDamageTracking() override;
    QSize size() const override;
    uint32_t format() const override;
    uint64_t modifier() const override;

    std::shared_ptr<EglContext> m_context;
    std::shared_ptr<EglSwapchain> m_swapchain;
    std::shared_ptr<EglSwapchainSlot> m_currentSlot;
};

struct DrmFormat
{
    uint32_t format;
    ModifierList modifiers;
};

static std::optional<DrmFormat> chooseFormat(uint32_t inputFormat, const FormatModifierMap &srcFormats, const FormatModifierMap &dstFormats)
{
    const auto modifiers = srcFormats[inputFormat].intersected(dstFormats[inputFormat]);
    if (!modifiers.empty()) {
        return DrmFormat{
            .format = inputFormat,
            .modifiers = modifiers,
        };
    }

    const auto info = FormatInfo::get(inputFormat).value_or(*FormatInfo::get(DRM_FORMAT_ARGB8888));
    std::optional<DrmFormat> ret;
    std::optional<uint32_t> retBPP;
    for (auto it = srcFormats.begin(); it != srcFormats.end(); it++) {
        const auto otherInfo = FormatInfo::get(it.key());
        // TODO sort the formats and accept suboptimal ones when needed
        if (!otherInfo.has_value() || otherInfo->bitsPerColor < info.bitsPerColor || otherInfo->alphaBits < info.alphaBits) {
            continue;
        }
        if (!retBPP || otherInfo->bitsPerPixel < retBPP) {
            const uint32_t fmt = it.key();
            const auto mods = srcFormats[fmt].intersected(dstFormats[fmt]);
            if (!mods.empty()) {
                ret = DrmFormat{
                    .format = fmt,
                    .modifiers = mods,
                };
                retBPP = otherInfo->bitsPerPixel;
            }
        }
    }
    return ret;
}

struct Ret
{
    std::unique_ptr<MultiGpuCopy> copy;
    ModifierList sourceModifiers;
};
static std::optional<Ret> createCopy(RenderDevice *device,
                                     uint32_t sourceFormat,
                                     const ModifierList &sourceModifiers,
                                     GraphicsBufferAllocator *allocator,
                                     const QSize &size,
                                     const FormatModifierMap &formats,
                                     bool scanout)
{
    // use Vulkan if possible
    if (device->vulkanDevice()) {
        auto retModifiers = device->vulkanDevice()->supportedFormats()[sourceFormat].intersected(sourceModifiers);
        if (!retModifiers.empty()) {
            auto fmt = chooseFormat(sourceFormat, device->vulkanDevice()->supportedFormats(), formats);
            if (fmt) {
                auto swapchain = VulkanSwapchain::create(device->vulkanDevice(), allocator, size, fmt->format, fmt->modifiers, scanout);
                if (swapchain) {
                    return Ret{
                        .copy = std::make_unique<VulkanMultiGpuCopy>(device, std::move(swapchain)),
                        .sourceModifiers = retModifiers,
                    };
                }
            }
        }
    }
    // fall back to EGL if not
    auto retModifiers = device->eglDisplay()->allSupportedDrmFormats()[sourceFormat].intersected(sourceModifiers);
    if (retModifiers.empty()) {
        return std::nullopt;
    }
    auto fmt = chooseFormat(sourceFormat, device->eglDisplay()->nonExternalOnlySupportedDrmFormats(), formats);
    if (!fmt) {
        return std::nullopt;
    }
    // creating the copy context will make it current
    const auto restoreContext = qScopeGuard([ctx = EglContext::currentContext()]() {
        if (ctx) {
            ctx->makeCurrent();
        }
    });
    auto context = device->eglContext();
    if (!context) {
        return std::nullopt;
    }
    auto swapchain = EglSwapchain::create(allocator, context.get(), size, fmt->format, fmt->modifiers, scanout);
    if (!swapchain) {
        return std::nullopt;
    }
    return Ret{
        .copy = std::make_unique<EglMultiGpuCopy>(device, std::move(context), std::move(swapchain)),
        .sourceModifiers = retModifiers,
    };
}

// There's a few constraints that decide which copy path we should take:
// - the kernel may implicitly migrate buffers to system memory when they're imported
//   into different GPUs, which can worsen performance significantly
// - for scanout, the destination buffer must be local, so it cannot ever be imported into the source device
// - we can't always directly access buffers on other GPUs, because of format/modifier
//   mismatches or simply missing hardware capabilities
// - importing a buffer allocated on Nvidia into another GPU causes GPU resets,
//   see https://github.com/NVIDIA/open-gpu-kernel-modules/issues/1037

std::unique_ptr<MultiGpuSwapchain> MultiGpuSwapchain::createForSampling(RenderDevice *sourceDevice, DrmDevice *targetDevice, uint32_t format, uint64_t modifier, const QSize &size, const FormatModifierMap &importFormats)
{
    RenderDevice *destination = GpuManager::self()->compatibleRenderDevice(targetDevice);
    if (sourceDevice->isInReset() || (destination && destination->isInReset())) {
        // avoid creating a suboptimal swapchain while in a reset
        return nullptr;
    }

    if (!sourceDevice->allImportableFormats().containsFormat(format, modifier)) {
        // This should never happen
        return nullptr;
    }

    GraphicsBufferAllocator *sysMemAllocator = GpuManager::self()->udmabufAllocator() ? GpuManager::self()->udmabufAllocator() : sourceDevice->drmDevice()->allocator();
    if (modifier == DRM_FORMAT_MOD_INVALID) {
        // Since implicit modifiers can't be relied on for cross-GPU access, first
        // do a copy to a linear buffer and only then the copy to the destination GPU
        auto firstCopy = createCopy(sourceDevice, format, {modifier}, sysMemAllocator, size, destination->allImportableFormats(), false);
        if (!firstCopy) {
            return nullptr;
        }
        auto secondCopy = createCopy(sourceDevice, firstCopy->copy->format(), {firstCopy->copy->modifier()}, targetDevice->allocator(), size, importFormats, false);
        if (!secondCopy) {
            return nullptr;
        }
        return std::make_unique<MultiGpuSwapchain>(std::move(firstCopy->copy), std::move(secondCopy->copy));
    } else {
        auto firstCopy = createCopy(sourceDevice, format, {modifier}, sysMemAllocator, size, importFormats, false);
        if (!firstCopy) {
            return nullptr;
        }
        return std::make_unique<MultiGpuSwapchain>(std::move(firstCopy->copy), nullptr);
    }
}

std::optional<MultiGpuSwapchain::AllocationInfo> MultiGpuSwapchain::createForScanout(RenderDevice *sourceDevice, DrmDevice *targetDevice,
                                                                                     uint32_t format, const ModifierList &modifiers,
                                                                                     const QSize &size, const FormatModifierMap &importFormats)
{
    RenderDevice *destination = GpuManager::self()->compatibleRenderDevice(targetDevice);
    if (sourceDevice->isInReset() || (destination && destination->isInReset())) {
        // avoid creating a suboptimal swapchain while in a reset
        return std::nullopt;
    }

    if (!destination) {
        // TODO this should fall back to CPU copy
        return std::nullopt;
    }

    // The destination buffer must not be migrated, or scanout will fail.
    if (sourceDevice->isInternal()) {
        auto retModifiers = destination->allImportableFormats()[format].intersected(modifiers);
        if (!retModifiers.isEmpty()) {
            // We can use the source buffer directly, since it's already in system memory.
            auto copy = createCopy(destination, format, modifiers, targetDevice->allocator(), size, importFormats, true);
            if (copy) {
                return AllocationInfo{
                    .swapchain = std::make_unique<MultiGpuSwapchain>(std::move(copy->copy), nullptr),
                    .importModifiers = copy->sourceModifiers,
                };
            }
        }
        // If there's no matching formats, fall back to double copy
    }

    // The two copies are required on dedicated GPUs to
    // avoid migrating the source buffer to system memory
    GraphicsBufferAllocator *sysMemAllocator = GpuManager::self()->udmabufAllocator() ? GpuManager::self()->udmabufAllocator() : sourceDevice->drmDevice()->allocator();
    auto firstCopy = createCopy(sourceDevice, format, modifiers, sysMemAllocator, size, destination->allImportableFormats(), false);
    if (!firstCopy) {
        return std::nullopt;
    }
    auto secondCopy = createCopy(destination, format, {firstCopy->copy->modifier()}, targetDevice->allocator(), size, importFormats, true);
    if (!secondCopy) {
        return std::nullopt;
    }
    return AllocationInfo{
        .swapchain = std::make_unique<MultiGpuSwapchain>(std::move(firstCopy->copy), std::move(secondCopy->copy)),
        .importModifiers = firstCopy->sourceModifiers,
    };
}

MultiGpuSwapchain::MultiGpuSwapchain(std::unique_ptr<MultiGpuCopy> &&firstCopy, std::unique_ptr<MultiGpuCopy> &&secondCopy)
    : m_firstCopy(std::move(firstCopy))
    , m_secondCopy(std::move(secondCopy))
    , m_format(m_secondCopy ? m_secondCopy->format() : m_firstCopy->format())
    , m_modifier(m_secondCopy ? m_secondCopy->modifier() : m_firstCopy->modifier())
    , m_size(m_firstCopy->size())
{
    connect(GpuManager::self(), &GpuManager::renderDeviceRemoved, this, &MultiGpuSwapchain::handleDeviceRemoved);
    connect(m_firstCopy.get(), &MultiGpuCopy::gpuReset, this, &MultiGpuSwapchain::handleGpuReset);
    if (m_secondCopy) {
        connect(m_secondCopy.get(), &MultiGpuCopy::gpuReset, this, &MultiGpuSwapchain::handleGpuReset);
    }
}

MultiGpuSwapchain::~MultiGpuSwapchain()
{
}

std::optional<MultiGpuSwapchain::Ret> MultiGpuSwapchain::copyRgbBuffer(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame,
                                                                       const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    if (!m_firstCopy) {
        return std::nullopt;
    }
    auto ret = m_firstCopy->copy(buffer, damage, std::move(sync), frame, releasePoint);
    if (m_secondCopy && ret) {
        ret = m_secondCopy->copy(ret->buffer, damage, std::move(ret->sync), frame, std::move(ret->releasePoint));
    }
    return ret;
}

MultiGpuCopy::MultiGpuCopy(RenderDevice *device)
    : m_device(device)
{
}

VulkanMultiGpuCopy::VulkanMultiGpuCopy(RenderDevice *device,
                                       std::unique_ptr<VulkanSwapchain> &&swapchain)
    : MultiGpuCopy(device)
    , m_swapchain(std::move(swapchain))
{
    connect(m_device->vulkanDevice(), &VulkanDevice::deviceLost, this, &MultiGpuCopy::gpuReset);
}

std::optional<MultiGpuSwapchain::Ret> VulkanMultiGpuCopy::copy(GraphicsBuffer *buffer,
                                                               const Region &damage,
                                                               FileDescriptor &&sync,
                                                               OutputFrame *frame,
                                                               const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    if (damage.isEmpty() && m_currentSlot) {
        return MultiGpuSwapchain::Ret{
            .buffer = m_currentSlot->buffer(),
            .sync = m_currentSlot->releaseFd().duplicate(),
            .releasePoint = m_currentSlot->releasePoint(),
        };
    }

    const auto copyVk = m_device->vulkanDevice();
    const auto srcTexture = copyVk->importBuffer(buffer, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (!srcTexture) {
        qCWarning(KWIN_VULKAN, "Could not import buffer for multi GPU copy!");
        m_journal.clear();
        return std::nullopt;
    }

    m_currentSlot = m_swapchain->acquire();
    if (!m_currentSlot) {
        qCWarning(KWIN_VULKAN, "Swapchain acquire failed!");
        m_journal.clear();
        return std::nullopt;
    }

    const Rect completeRect{QPoint(), m_swapchain->size()};
    const Region toRender = (m_journal.accumulate(m_currentSlot->age(), completeRect) | damage) & completeRect;
    if (toRender.isEmpty()) {
        return MultiGpuSwapchain::Ret{
            .buffer = m_currentSlot->buffer(),
            .sync = m_currentSlot->releaseFd().duplicate(),
            .releasePoint = m_currentSlot->releasePoint(),
        };
    }
    m_journal.add(damage);

    auto commandBuffer = copyVk->createCommandBuffer();
    vk::Result result = commandBuffer.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    if (result != vk::Result::eSuccess) {
        m_journal.clear();
        return std::nullopt;
    }

    std::unique_ptr<VulkanRenderTimeQuery> query;
    if (frame) {
        query = VulkanRenderTimeQuery::begin(copyVk, commandBuffer, copyVk->transferQueueFamily());
    }

    vk::ImageMemoryBarrier2 memoryBarrier{
        vk::PipelineStageFlagBits2::eAllCommands,
        vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
        vk::PipelineStageFlagBits2::eAllCommands,
        vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
        vk::ImageLayout::eGeneral,
        vk::ImageLayout::eGeneral,
        vk::QueueFamilyExternal,
        copyVk->transferQueueFamily(),
        m_currentSlot->texture()->handle(),
        vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1,
        },
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        vk::DependencyFlags{},
        {},
        {},
        memoryBarrier,
    });

    const std::vector<vk::ImageBlit> regions = toRender.rects() | std::views::transform([&completeRect](const Rect &rect) {
        return vk::ImageBlit{
            // src
            vk::ImageSubresourceLayers{
                vk::ImageAspectFlagBits::eColor,
                0,
                0,
                1,
            },
            std::array{
                vk::Offset3D{rect.left(), completeRect.height() - rect.bottom(), 0},
                vk::Offset3D{rect.right(), completeRect.height() - rect.top(), 1},
            },
            // dst
            vk::ImageSubresourceLayers{
                vk::ImageAspectFlagBits::eColor,
                0,
                0,
                1,
            },
            std::array{
                vk::Offset3D{rect.left(), completeRect.height() - rect.bottom(), 0},
                vk::Offset3D{rect.right(), completeRect.height() - rect.top(), 1},
            },
        };
    }) | std::ranges::to<std::vector>();
    commandBuffer.blitImage(srcTexture->handle(), vk::ImageLayout::eGeneral,
                            m_currentSlot->texture()->handle(), vk::ImageLayout::eGeneral,
                            regions, vk::Filter::eNearest);

    memoryBarrier.setSrcQueueFamilyIndex(copyVk->transferQueueFamily());
    memoryBarrier.setDstQueueFamilyIndex(vk::QueueFamilyExternal);
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        vk::DependencyFlags{},
        {},
        {},
        memoryBarrier,
    });

    if (query) {
        query->end(commandBuffer);
        frame->addRenderTimeQuery(std::move(query));
    }

    result = commandBuffer.end();
    if (result != vk::Result::eSuccess) {
        m_journal.clear();
        return std::nullopt;
    }
    auto completionFd = copyVk->submit(std::move(commandBuffer), std::move(sync));
    if (!completionFd.has_value()) {
        return std::nullopt;
    }
    if (releasePoint) {
        releasePoint->addReleaseFence(*completionFd);
    }
    m_swapchain->release(m_currentSlot.get(), completionFd->duplicate());
    return MultiGpuSwapchain::Ret{
        .buffer = m_currentSlot->buffer(),
        .sync = std::move(*completionFd),
        .releasePoint = m_currentSlot->releasePoint(),
    };
}

void VulkanMultiGpuCopy::resetDamageTracking()
{
    m_currentSlot.reset();
    m_swapchain->resetBufferAge();
    m_journal.clear();
}

QSize VulkanMultiGpuCopy::size() const
{
    return m_swapchain->size();
}

uint32_t VulkanMultiGpuCopy::format() const
{
    return m_swapchain->format();
}

uint64_t VulkanMultiGpuCopy::modifier() const
{
    return m_swapchain->modifier();
}

EglMultiGpuCopy::EglMultiGpuCopy(RenderDevice *device,
                                 std::shared_ptr<EglContext> &&context,
                                 std::shared_ptr<EglSwapchain> &&swapchain)
    : MultiGpuCopy(device)
    , m_context(std::move(context))
    , m_swapchain(std::move(swapchain))
{
}

EglMultiGpuCopy::~EglMultiGpuCopy()
{
    auto previousContext = EglContext::currentContext();
    m_context->makeCurrent();
    m_currentSlot.reset();
    m_swapchain.reset();
    if (previousContext) {
        previousContext->makeCurrent();
    }
}

std::optional<MultiGpuSwapchain::Ret> EglMultiGpuCopy::copy(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame,
                                                            const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    if (damage.isEmpty() && m_currentSlot) {
        return MultiGpuSwapchain::Ret{
            .buffer = m_currentSlot->buffer(),
            .sync = m_currentSlot->releaseFd().duplicate(),
            .releasePoint = m_currentSlot->releasePoint(),
        };
    }

    EglContext *previousContext = EglContext::currentContext();
    const auto restoreContext = qScopeGuard([previousContext]() {
        if (previousContext) {
            // TODO make the calling code responsible for this?
            // If this makeCurrent fails, things might crash :/
            previousContext->makeCurrent();
        }
    });
    if (!m_context || m_context->isFailed() || !m_context->makeCurrent()) {
        Q_EMIT gpuReset();
        return std::nullopt;
    }
    std::unique_ptr<GLRenderTimeQuery> renderTime;
    if (frame) {
        renderTime = std::make_unique<GLRenderTimeQuery>(m_context);
        renderTime->begin();
    }
    m_currentSlot = m_swapchain->acquire();
    if (!m_currentSlot) {
        m_journal.clear();
        return std::nullopt;
    }
    auto sourceTex = m_context->importDmaBufAsTexture(*buffer->dmabufAttributes());
    if (!sourceTex) {
        m_journal.clear();
        return std::nullopt;
    }

    const Rect completeRect{QPoint(), m_swapchain->size()};
    const Region toRender = (m_journal.accumulate(m_currentSlot->age(), completeRect) | damage) & completeRect;
    m_journal.add(damage);

    m_context->pushFramebuffer(m_currentSlot->framebuffer());
    // TODO when possible, use a blit instead of a shader for better performance?
    ShaderBinder binder(sourceTex->target() == GL_TEXTURE_EXTERNAL_OES ? ShaderTrait::MapExternalTexture : ShaderTrait::MapTexture);
    QMatrix4x4 proj;
    proj.scale(1, -1);
    proj.ortho(QRectF(QPointF(), buffer->size()));
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, proj);
    sourceTex->render(toRender, buffer->size());
    m_context->popFramebuffer();
    EGLNativeFence fence(m_context->displayObject());
    m_swapchain->release(m_currentSlot, fence.fileDescriptor().duplicate());

    // destroy resources before the context switch
    sourceTex.reset();
    if (renderTime) {
        renderTime->end();
        frame->addRenderTimeQuery(std::move(renderTime));
    }
    if (releasePoint) {
        releasePoint->addReleaseFence(fence.fileDescriptor());
    }
    return MultiGpuSwapchain::Ret{
        .buffer = m_currentSlot->buffer(),
        .sync = fence.takeFileDescriptor(),
        .releasePoint = m_currentSlot->releasePoint(),
    };
}

void EglMultiGpuCopy::resetDamageTracking()
{
    m_currentSlot.reset();
    m_swapchain->resetBufferAge();
    m_journal.clear();
}

QSize EglMultiGpuCopy::size() const
{
    return m_swapchain->size();
}

uint32_t EglMultiGpuCopy::format() const
{
    return m_swapchain->format();
}

uint64_t EglMultiGpuCopy::modifier() const
{
    return m_swapchain->modifier();
}

void MultiGpuSwapchain::handleDeviceRemoved(RenderDevice *device)
{
    if (!m_firstCopy) {
        return;
    }
    if (m_firstCopy->m_device == device || (m_secondCopy && m_secondCopy->m_device == device)) {
        m_firstCopy.reset();
        m_secondCopy.reset();
    }
}

void MultiGpuSwapchain::resetDamageTracking()
{
    if (m_firstCopy) {
        m_firstCopy->resetDamageTracking();
    }
    if (m_secondCopy) {
        m_secondCopy->resetDamageTracking();
    }
}

void MultiGpuSwapchain::handleGpuReset()
{
    m_firstCopy.reset();
    m_secondCopy.reset();
    m_needsRecreation = true;
}

uint32_t MultiGpuSwapchain::format() const
{
    return m_format;
}

uint64_t MultiGpuSwapchain::modifier() const
{
    return m_modifier;
}

QSize MultiGpuSwapchain::size() const
{
    return m_size;
}

bool MultiGpuSwapchain::needsRecreation() const
{
    return m_needsRecreation;
}

}

#include "moc_multigpuswapchain.cpp"
#include "multigpuswapchain.moc"

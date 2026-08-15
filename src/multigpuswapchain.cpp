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
    explicit EglMultiGpuCopy(RenderDevice *device, std::shared_ptr<EglSwapchain> &&swapchain);
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
    auto modifiers = srcFormats[inputFormat].intersected(dstFormats[inputFormat]);
    modifiers.erase(DRM_FORMAT_MOD_INVALID);
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
            auto mods = srcFormats[fmt].intersected(dstFormats[fmt]);
            mods.erase(DRM_FORMAT_MOD_INVALID);
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

static const auto s_forceLinear = environmentVariableBoolValue("KWIN_VULKAN_FORCE_LINEAR_DST");

struct CopyRet
{
    std::unique_ptr<MultiGpuCopy> copy;
    ModifierList sourceModifiers;
};
static std::optional<CopyRet> createCopy(RenderDevice *device,
                                         uint32_t sourceFormat,
                                         const ModifierList &sourceModifiers,
                                         GraphicsBufferAllocator *allocator,
                                         const FormatModifierMap &formats,
                                         GraphicsBufferOptions options)
{
    // use Vulkan if possible
    if (device->vulkanDevice()) {
        auto retModifiers = device->vulkanDevice()->transferFormats()[sourceFormat].intersected(sourceModifiers);
        if (!retModifiers.empty()) {
            const auto fmt = chooseFormat(sourceFormat, device->vulkanDevice()->transferFormats(), formats);
            if (fmt) {
                options.format = fmt->format;
                options.modifiers = fmt->modifiers;
                options.render = false;
                auto swapchain = VulkanSwapchain::create(device->vulkanDevice(), allocator, options);
                if (swapchain) {
                    return CopyRet{
                        .copy = std::make_unique<VulkanMultiGpuCopy>(device, std::move(swapchain)),
                        .sourceModifiers = retModifiers,
                    };
                }
            }
        }
    }
    // fall back to EGL if not
    const auto retModifiers = device->eglDisplay()->allSupportedDrmFormats()[sourceFormat].intersected(sourceModifiers);
    if (retModifiers.empty()) {
        return std::nullopt;
    }
    const auto fmt = chooseFormat(sourceFormat, device->eglDisplay()->nonExternalOnlySupportedDrmFormats(), formats);
    if (!fmt) {
        return std::nullopt;
    }
    // creating the copy context will make it current
    const auto restoreContext = qScopeGuard([ctx = EglContext::currentContext()]() {
        if (ctx) {
            (void)ctx->makeCurrent();
        }
    });
    auto context = device->eglContext();
    if (!context || !context->makeCurrent()) {
        return std::nullopt;
    }
    options.format = fmt->format;
    options.modifiers = fmt->modifiers;
    options.render = true;
    auto swapchain = EglSwapchain::create(allocator, context, options);
    if (!swapchain) {
        return std::nullopt;
    }
    return CopyRet{
        .copy = std::make_unique<EglMultiGpuCopy>(device, std::move(swapchain)),
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

std::unique_ptr<MultiGpuSwapchain> MultiGpuSwapchain::createForSampling(RenderDevice *sourceDevice, RenderDevice *destination, uint32_t format, uint64_t modifier, const QSize &size, const FormatModifierMap &importFormats)
{
    if (sourceDevice->isInReset() || destination->isInReset()) {
        // avoid creating a suboptimal swapchain while in a reset
        return nullptr;
    }

    if (!sourceDevice->allImportableFormats().containsFormat(format, modifier)) {
        // This should never happen
        return nullptr;
    }

    GraphicsBufferOptions options{
        .size = size,
        .software = false,
        .scanout = false,
    };

    RenderDevice *sysMemDevice = GpuManager::self()->softwareDevice();

    // NOTE that udmabuf has an arbitrary size limit fo 64MB as of Linux 7.2,
    // so we need to have a fallback when creating the udmabuf fails.
    // Additionally, importing a dmabuf from Nvidia into a non-Nvidia GPU
    // will cause a hang because of a bug in the Nvidia driver:
    // https://github.com/NVIDIA/open-gpu-kernel-modules/issues/1037
    RenderDevice *fallbackDevice = !sourceDevice->drmDevice() || sourceDevice->drmDevice()->isNvidia() ? destination : sourceDevice;

    std::optional<KWin::CopyRet> firstCopy;

    // For implicit modifiers, we need to do a second copy, since accessing
    // them from multiple GPUs may not work as we expect.
    // For dedicated GPUs, the second copy improves performance.
    if (modifier == DRM_FORMAT_MOD_INVALID || !destination->isInternal()) {
        if (sysMemDevice) {
            firstCopy = createCopy(sourceDevice, format, {modifier}, sysMemDevice->allocator(), destination->allImportableFormats(), options);
        }
        if (!firstCopy) {
            firstCopy = createCopy(sourceDevice, format, {modifier}, fallbackDevice->allocator(), destination->allImportableFormats(), options);
        }
        if (!firstCopy) {
            return nullptr;
        }
        auto secondCopy = createCopy(destination, firstCopy->copy->format(), {firstCopy->copy->modifier()}, destination->allocator(), importFormats, options);
        if (!secondCopy) {
            return nullptr;
        }
        return std::make_unique<MultiGpuSwapchain>(std::move(firstCopy->copy), std::move(secondCopy->copy), format);
    } else {
        if (sysMemDevice) {
            firstCopy = createCopy(sourceDevice, format, {modifier}, sysMemDevice->allocator(), importFormats, options);
        }
        if (!firstCopy) {
            firstCopy = createCopy(sourceDevice, format, {modifier}, fallbackDevice->allocator(), importFormats, options);
        }
        if (!firstCopy) {
            return nullptr;
        }
        return std::make_unique<MultiGpuSwapchain>(std::move(firstCopy->copy), nullptr, format);
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

    if (!sourceDevice->allImportableFormats().contains(format)) {
        // This should never happen
        return std::nullopt;
    }

    GraphicsBufferOptions options{
        .size = size,
        .software = false,
        .scanout = false,
    };

    // The destination buffer must not be migrated, or scanout will fail.
    if (sourceDevice->isInternal()) {
        auto retModifiers = destination->allImportableFormats()[format].intersected(modifiers);
        if (!retModifiers.isEmpty()) {
            // We can use the source buffer directly, since it's already in system memory.
            options.scanout = true;
            auto copy = createCopy(destination, format, modifiers, targetDevice->allocator(), importFormats, options);
            if (copy) {
                return AllocationInfo{
                    .swapchain = std::make_unique<MultiGpuSwapchain>(std::move(copy->copy), nullptr, format),
                    .importModifiers = copy->sourceModifiers,
                };
            }
        }
        // If there's no matching formats, fall back to double copy
    }

    RenderDevice *sysMemDevice = GpuManager::self()->softwareDevice();
    RenderDevice *fallbackDevice = !sourceDevice->drmDevice() || sourceDevice->drmDevice()->isNvidia() ? destination : sourceDevice;

    options.scanout = false;
    std::optional<KWin::CopyRet> firstCopy;
    if (sysMemDevice) {
        firstCopy = createCopy(sourceDevice, format, modifiers, sysMemDevice->allocator(), destination->allImportableFormats(), options);
    }
    if (!firstCopy) {
        firstCopy = createCopy(sourceDevice, format, modifiers, fallbackDevice->allocator(), destination->allImportableFormats(), options);
    }
    if (!firstCopy) {
        return std::nullopt;
    }
    options.scanout = true;
    auto secondCopy = createCopy(destination, format, {firstCopy->copy->modifier()}, targetDevice->allocator(), importFormats, options);
    if (!secondCopy) {
        return std::nullopt;
    }
    return AllocationInfo{
        .swapchain = std::make_unique<MultiGpuSwapchain>(std::move(firstCopy->copy), std::move(secondCopy->copy), format),
        .importModifiers = firstCopy->sourceModifiers,
    };
}

MultiGpuSwapchain::MultiGpuSwapchain(std::unique_ptr<MultiGpuCopy> &&firstCopy, std::unique_ptr<MultiGpuCopy> &&secondCopy, uint32_t sourceFormat)
    : m_firstCopy(std::move(firstCopy))
    , m_secondCopy(std::move(secondCopy))
    , m_format(m_secondCopy ? m_secondCopy->format() : m_firstCopy->format())
    , m_modifier(m_secondCopy ? m_secondCopy->modifier() : m_firstCopy->modifier())
    , m_size(m_firstCopy->size())
    , m_sourceFormat(sourceFormat)
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
    const bool useTransferQueue = copyVk->transferQueue()
        && FormatInfo::get(buffer->dmabufAttributes()->format)->bitsPerPixel == FormatInfo::get(m_swapchain->format())->bitsPerPixel;
    const auto queue = useTransferQueue ? copyVk->transferQueue() : copyVk->graphicsQueue();

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

    auto commandBuffer = queue->createCommandBuffer();
    vk::Result result = commandBuffer.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    if (result != vk::Result::eSuccess) {
        m_journal.clear();
        return std::nullopt;
    }

    std::unique_ptr<VulkanRenderTimeQuery> query;
    if (frame) {
        query = VulkanRenderTimeQuery::begin(copyVk, commandBuffer, queue->familyIndex());
    }

    std::array<vk::ImageMemoryBarrier2, 2> memoryBarriers = {
        vk::ImageMemoryBarrier2{
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eGeneral,
            vk::QueueFamilyExternal,
            queue->familyIndex(),
            m_currentSlot->texture()->handle(),
            vk::ImageSubresourceRange{
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
            },
        },
        vk::ImageMemoryBarrier2{
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eGeneral,
            vk::QueueFamilyExternal,
            queue->familyIndex(),
            srcTexture->handle(),
            vk::ImageSubresourceRange{
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
            },
        },
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        vk::DependencyFlags{},
        {},
        {},
        memoryBarriers,
    });

    if (useTransferQueue) {
        const std::vector<vk::ImageCopy> regions = toRender.rects() | std::views::transform([](const Rect &rect) {
            return vk::ImageCopy{
                // src
                vk::ImageSubresourceLayers{
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1,
                },
                vk::Offset3D{rect.left(), rect.top(), 0},
                // dst
                vk::ImageSubresourceLayers{
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1,
                },
                vk::Offset3D{rect.left(), rect.top(), 0},
                vk::Extent3D{uint32_t(rect.width()), uint32_t(rect.height()), 1},
            };
        }) | std::ranges::to<std::vector>();
        commandBuffer.copyImage(srcTexture->handle(), vk::ImageLayout::eGeneral,
                                m_currentSlot->texture()->handle(), vk::ImageLayout::eGeneral,
                                regions);
    } else {
        const std::vector<vk::ImageBlit> regions = toRender.rects() | std::views::transform([](const Rect &rect) {
            return vk::ImageBlit{
                // src
                vk::ImageSubresourceLayers{
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1,
                },
                std::array{
                    vk::Offset3D{rect.left(), rect.top(), 0},
                    vk::Offset3D{rect.right(), rect.bottom(), 1},
                },
                // dst
                vk::ImageSubresourceLayers{
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    0,
                    1,
                },
                std::array{
                    vk::Offset3D{rect.left(), rect.top(), 0},
                    vk::Offset3D{rect.right(), rect.bottom(), 1},
                },
            };
        }) | std::ranges::to<std::vector>();
        commandBuffer.blitImage(srcTexture->handle(), vk::ImageLayout::eGeneral,
                                m_currentSlot->texture()->handle(), vk::ImageLayout::eGeneral,
                                regions, vk::Filter::eNearest);
    }

    for (auto &barrier : memoryBarriers) {
        barrier.setSrcQueueFamilyIndex(queue->familyIndex());
        barrier.setDstQueueFamilyIndex(vk::QueueFamilyExternal);
    }
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        vk::DependencyFlags{},
        {},
        {},
        memoryBarriers,
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
    auto completionFd = queue->submit(std::move(commandBuffer), std::move(sync), {buffer, m_currentSlot->buffer()});
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

EglMultiGpuCopy::EglMultiGpuCopy(RenderDevice *device, std::shared_ptr<EglSwapchain> &&swapchain)
    : MultiGpuCopy(device)
    , m_swapchain(std::move(swapchain))
{
}

EglMultiGpuCopy::~EglMultiGpuCopy()
{
    auto previousContext = EglContext::currentContext();
    m_currentSlot.reset();
    m_swapchain.reset();
    if (previousContext) {
        (void)previousContext->makeCurrent();
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
            (void)previousContext->makeCurrent();
        }
    });
    if (!m_swapchain || m_swapchain->context()->isFailed() || !m_swapchain->context()->makeCurrent()) {
        Q_EMIT gpuReset();
        return std::nullopt;
    }

    const auto &context = m_swapchain->context();

    std::unique_ptr<GLRenderTimeQuery> renderTime;
    if (frame) {
        renderTime = std::make_unique<GLRenderTimeQuery>(context);
        renderTime->begin();
    }
    m_currentSlot = m_swapchain->acquire();
    if (!m_currentSlot) {
        m_journal.clear();
        return std::nullopt;
    }
    auto sourceTex = context->importDmaBufAsTexture(*buffer->dmabufAttributes());
    if (!sourceTex) {
        m_journal.clear();
        return std::nullopt;
    }

    const Rect completeRect{QPoint(), m_swapchain->size()};
    // GLVertexBuffer flips the clip region vertically. In other words, it maps (0, 0) to the
    // top-left corner of the render target. It does so because it's more convenient in the
    // rendering code.
    //
    // However, the input damage region is specified in the final graphics buffer coordinates,
    // with the origin in the top-left corner. In other words, damage = flipVertically(toRender),
    // so we apply a flip-y transform to get a toRender region so when GLVertexBuffer flips it,
    // we get the original input damage region.
    const Region toRender = OutputTransform(OutputTransform::FlipY).map((m_journal.accumulate(m_currentSlot->age(), completeRect) | damage) & completeRect, m_swapchain->size());
    m_journal.add(damage);

    context->pushFramebuffer(m_currentSlot->framebuffer());
    ShaderBinder binder(sourceTex->target() == GL_TEXTURE_EXTERNAL_OES ? ShaderTrait::MapExternalTexture : ShaderTrait::MapTexture);
    QMatrix4x4 proj;
    proj.scale(1, -1);
    proj.ortho(QRectF(QPointF(), buffer->size()));
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, proj);

    glEnable(GL_SCISSOR_TEST);
    sourceTex->render(toRender, buffer->size(), true);
    glDisable(GL_SCISSOR_TEST);

    context->popFramebuffer();
    EGLNativeFence fence(context->displayObject());
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

bool MultiGpuSwapchain::isSuitableFor(GraphicsBuffer *buffer) const
{
    if (!m_firstCopy || needsRecreation()) {
        return false;
    }
    const auto attrs = buffer->dmabufAttributes();
    if (!attrs || attrs->format != m_sourceFormat || buffer->size() != m_size) {
        return false;
    }
    if (qobject_cast<VulkanMultiGpuCopy *>(m_firstCopy.get())) {
        return m_firstCopy->m_device->vulkanDevice()->transferFormats().containsFormat(attrs->format, attrs->modifier);
    } else {
        return m_firstCopy->m_device->eglDisplay()->allSupportedDrmFormats().containsFormat(attrs->format, attrs->modifier);
    }
}

}

#include "moc_multigpuswapchain.cpp"
#include "multigpuswapchain.moc"

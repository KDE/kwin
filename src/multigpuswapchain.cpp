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
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/glrendertimequery.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "utils/envvar.h"

namespace KWin
{

struct DrmFormat
{
    uint32_t format;
    ModifierList modifiers;
};

static std::optional<DrmFormat> chooseFormat(uint32_t inputFormat, const FormatModifierMap &srcFormats, const FormatModifierMap &dstFormats)
{
    if (srcFormats.contains(inputFormat)) {
        return DrmFormat{
            .format = inputFormat,
            .modifiers = srcFormats[inputFormat].intersected(dstFormats[inputFormat]),
        };
    }
    const auto info = FormatInfo::get(inputFormat).value_or(*FormatInfo::get(DRM_FORMAT_ARGB8888));
    std::optional<DrmFormat> ret;
    std::optional<uint32_t> retBPP;
    for (auto it = srcFormats.begin(); it != srcFormats.end(); it++) {
        const auto otherInfo = FormatInfo::get(it.key());
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

std::unique_ptr<MultiGpuSwapchain> MultiGpuSwapchain::create(RenderDevice *copyDevice, DrmDevice *targetDevice, uint32_t format, uint64_t modifier, const QSize &size, const FormatModifierMap &importFormats)
{
    if (copyDevice->eglDisplay() && copyDevice->eglDisplay()->nonExternalOnlySupportedDrmFormats().containsFormat(format, modifier)) {
        const auto context = copyDevice->eglContext();
        if (!context) {
            return nullptr;
        }
        auto formatMod = chooseFormat(format, copyDevice->eglDisplay()->nonExternalOnlySupportedDrmFormats(), importFormats);
        if (!formatMod) {
            return nullptr;
        }
        auto eglSwapchain = EglSwapchain::create(copyDevice->drmDevice()->allocator(), context.get(), size, formatMod->format, formatMod->modifiers);
        if (eglSwapchain) {
            return std::make_unique<MultiGpuSwapchain>(copyDevice, targetDevice, context, std::move(eglSwapchain));
        }
    }
    return nullptr;
}

MultiGpuSwapchain::MultiGpuSwapchain(RenderDevice *copyDevice, DrmDevice *targetDevice, const std::shared_ptr<EglContext> &eglContext, std::shared_ptr<EglSwapchain> &&eglSwapchain)
    : m_targetDevice(targetDevice)
    , m_copyDevice(copyDevice)
    , m_copyContext(eglContext)
    , m_eglSwapchain(std::move(eglSwapchain))
    , m_format(m_eglSwapchain->format())
    , m_modifier(m_eglSwapchain->modifier())
    , m_size(m_eglSwapchain->size())
{
    connect(GpuManager::s_self.get(), &GpuManager::renderDeviceRemoved, this, &MultiGpuSwapchain::handleDeviceRemoved);
}

MultiGpuSwapchain::~MultiGpuSwapchain()
{
    deleteResources();
}

std::optional<MultiGpuSwapchain::Ret> MultiGpuSwapchain::copyRgbBuffer(GraphicsBuffer *buffer, const Region &damage, FileDescriptor &&sync, OutputFrame *frame)
{
    if (!m_copyDevice || !m_targetDevice || !buffer->dmabufAttributes()) {
        return std::nullopt;
    }

    if (damage.isEmpty() && m_currentEglSlot) {
        return Ret{
            .buffer = m_currentEglSlot->buffer(),
            .sync = m_currentEglSlot->releaseFd().duplicate(),
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
    if (!m_copyContext || m_copyContext->isFailed() || !m_copyContext->makeCurrent()) {
        m_copyContext = m_copyDevice->eglContext();
    }
    if (!m_copyContext) {
        m_journal.clear();
        return std::nullopt;
    }
    std::unique_ptr<GLRenderTimeQuery> renderTime;
    if (frame) {
        renderTime = std::make_unique<GLRenderTimeQuery>(m_copyContext);
        renderTime->begin();
    }
    m_currentEglSlot = m_eglSwapchain->acquire();
    if (!m_currentEglSlot) {
        m_journal.clear();
        return std::nullopt;
    }
    auto sourceTex = m_copyContext->importDmaBufAsTexture(*buffer->dmabufAttributes());
    if (!sourceTex) {
        m_journal.clear();
        return std::nullopt;
    }

    const Rect completeRect{QPoint(), m_size};
    const Region toRender = (m_journal.accumulate(m_currentEglSlot->age(), completeRect) | damage) & completeRect;
    m_journal.add(damage);

    m_copyContext->pushFramebuffer(m_currentEglSlot->framebuffer());
    // TODO when possible, use a blit instead of a shader for better performance?
    ShaderBinder binder(ShaderTrait::MapTexture);
    QMatrix4x4 proj;
    proj.scale(1, -1);
    proj.ortho(QRectF(QPointF(), buffer->size()));
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, proj);
    sourceTex->render(toRender, buffer->size());
    m_copyContext->popFramebuffer();
    EGLNativeFence fence(m_copyContext->displayObject());
    m_eglSwapchain->release(m_currentEglSlot, fence.fileDescriptor().duplicate());

    // destroy resources before the context switch
    sourceTex.reset();
    if (renderTime) {
        renderTime->end();
        frame->addRenderTimeQuery(std::move(renderTime));
    }
    return Ret{
        .buffer = m_currentEglSlot->buffer(),
        .sync = fence.takeFileDescriptor(),
    };
}

void MultiGpuSwapchain::handleDeviceRemoved(RenderDevice *device)
{
    if (m_copyDevice == device || m_targetDevice == device->drmDevice()) {
        deleteResources();
    }
}

void MultiGpuSwapchain::deleteResources()
{
    if (m_copyContext) {
        m_copyContext->makeCurrent();
        m_currentEglSlot.reset();
        m_eglSwapchain.reset();
        m_copyContext.reset();
    }
    m_copyDevice = nullptr;
    m_targetDevice = nullptr;
}

void MultiGpuSwapchain::resetDamageTracking()
{
    m_currentEglSlot.reset();
    if (m_eglSwapchain) {
        m_eglSwapchain->resetBufferAge();
    }
    m_journal.clear();
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
}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "eglmultigpuswapchain.h"
#include "core/drm_formats.h"
#include "core/drmdevice.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "core/renderdevice.h"
#include "eglcontext.h"
#include "egldisplay.h"
#include "eglnativefence.h"
#include "glframebuffer.h"
#include "glshader.h"
#include "glshadermanager.h"

namespace KWin
{

EglMultiGpuSwapchain::EglMultiGpuSwapchain(RenderDevice *sourceDevice,
                                           const std::shared_ptr<EglContext> &targetContext)
    : m_sourceDevice(sourceDevice)
    , m_sourceContext(sourceDevice->eglContext())
    , m_importContext(targetContext)
{
    connect(GpuManager::s_self.get(), &GpuManager::renderDeviceRemoved, this, &EglMultiGpuSwapchain::handleDeviceRemoved);
}

EglMultiGpuSwapchain::~EglMultiGpuSwapchain()
{
    m_sourceContext->makeCurrent();
    m_swapchain.reset();
    m_importContext->makeCurrent();
}

struct DrmFormat
{
    uint32_t format;
    ModifierList modifiers;
};

static std::optional<DrmFormat> chooseFormat(uint32_t inputFormat,
                                             const QHash<uint32_t, EglDisplay::DrmFormatInfo> &srcFormats,
                                             const QHash<uint32_t, EglDisplay::DrmFormatInfo> &dstFormats)
{
    if (srcFormats.contains(inputFormat)) {
        return DrmFormat{
            .format = inputFormat,
            .modifiers = intersect(srcFormats[inputFormat].nonExternalOnlyModifiers, dstFormats[inputFormat].allModifiers),
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
            const auto mods = intersect(srcFormats[fmt].nonExternalOnlyModifiers, dstFormats[fmt].allModifiers);
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

std::pair<GraphicsBuffer *, std::shared_ptr<ColorDescription>> EglMultiGpuSwapchain::importBuffer(GraphicsBuffer *buffer,
                                                                                                  const std::shared_ptr<ColorDescription> &color)
{
    Q_ASSERT(buffer->dmabufAttributes());
    const auto info = FormatInfo::get(buffer->dmabufAttributes()->format);
    std::optional<YuvConversion> conv = info ? info->yuvConversion() : std::nullopt;
    if (conv) {
        // FIXME add support for YUV stuff
        return std::make_pair(nullptr, color);
    }

    if (!m_sourceContext || !m_sourceContext->makeCurrent()) {
        Q_ASSERT(m_importContext->makeCurrent());
        return std::make_pair(nullptr, color);
    }

    if (m_lastFormat != buffer->dmabufAttributes()->format || !m_swapchain) {
        // copy to buffer allocated on source GPU, then import that on destination GPU
        auto formatMod = chooseFormat(buffer->dmabufAttributes()->format, m_sourceContext->displayObject()->allSupportedDrmFormats(),
                                      m_importContext->displayObject()->allSupportedDrmFormats());
        if (formatMod) {
            m_swapchain = EglSwapchain::create(m_sourceDevice->drmDevice()->allocator(), m_sourceContext.get(),
                                               buffer->size(), formatMod->format, formatMod->modifiers);
        } else {
            // FIXME implement (and benchmark) options for this code path:
            // 1. allocate shadow buffer on dst GPU, import that on src and copy on src
            // 2. import the buffer directly (may cause buffer migrations!)
            // 3. do a CPU copy
            Q_ASSERT(m_importContext->makeCurrent());
            return std::make_pair(nullptr, color);
        }
    }

    auto slot = m_swapchain->acquire();
    if (!slot) {
        Q_ASSERT(m_importContext->makeCurrent());
        return std::make_pair(nullptr, color);
    }
    auto sourceTex = m_sourceContext->importDmaBufAsTexture(*buffer->dmabufAttributes());
    if (!sourceTex) {
        Q_ASSERT(m_importContext->makeCurrent());
        return std::make_pair(nullptr, color);
    }
    m_sourceContext->pushFramebuffer(slot->framebuffer());
    // TODO when possible, use a blit instead of a shader for better performance
    ShaderBinder binder(ShaderTrait::MapTexture);
    QMatrix4x4 proj;
    proj.scale(1, -1);
    proj.ortho(QRectF(QPointF(), buffer->size()));
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, proj);
    sourceTex->render(buffer->size());
    m_sourceContext->popFramebuffer();
    {
        EGLNativeFence fence(m_sourceContext->displayObject());
        m_swapchain->release(slot, fence.takeFileDescriptor());
    }
    // destroy the texture before the context switch
    // TODO add some safe guards or asserts for this?
    // Figuring out this was causing issues took quite a long time...
    // TODO also, cache the texture?
    sourceTex.reset();

    // TODO this whole context switching mess will require being able
    // to cancel a compositing cycle for robust GPU reset handling :/
    Q_ASSERT(m_importContext->makeCurrent());
    return std::make_pair(slot->buffer(), color);
}

void EglMultiGpuSwapchain::handleDeviceRemoved(RenderDevice *device)
{
    if (m_sourceDevice == device) {
        m_sourceContext->makeCurrent();
        m_swapchain.reset();
        m_sourceContext.reset();
        m_sourceDevice = nullptr;
    }
}
}

#include "moc_eglmultigpuswapchain.cpp"

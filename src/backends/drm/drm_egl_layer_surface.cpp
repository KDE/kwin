/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer_surface.h"

#include "config-kwin.h"

#include "core/colortransformation.h"
#include "core/graphicsbufferview.h"
#include "core/iccprofile.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "opengl/eglnativefence.h"
#include "opengl/eglswapchain.h"
#include "opengl/gllut.h"
#include "opengl/glrendertimequery.h"
#include "opengl/icc_shader.h"
#include "qpainter/qpainterswapchain.h"
#include "utils/drm_format_helper.h"

#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

static const QList<uint64_t> linearModifier = {DRM_FORMAT_MOD_LINEAR};
static const QList<uint64_t> implicitModifier = {DRM_FORMAT_MOD_INVALID};
static const QList<uint32_t> cpuCopyFormats = {DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888};

static const bool bufferAgeEnabled = qEnvironmentVariable("KWIN_USE_BUFFER_AGE") != QStringLiteral("0");

static gbm_format_name_desc formatName(uint32_t format)
{
    gbm_format_name_desc ret;
    gbm_format_get_name(format, &ret);
    return ret;
}

EglGbmLayerSurface::EglGbmLayerSurface(DrmGpu *gpu, EglGbmBackend *eglBackend, BufferTarget target, FormatOption formatOption)
    : m_gpu(gpu)
    , m_eglBackend(eglBackend)
    , m_requestedBufferTarget(target)
    , m_formatOption(formatOption)
{
}

EglGbmLayerSurface::~EglGbmLayerSurface() = default;

EglGbmLayerSurface::Surface::~Surface()
{
    if (importContext) {
        importContext->makeCurrent();
        importGbmSwapchain.reset();
        importedTextureCache.clear();
        importContext.reset();
    }
    if (context) {
        context->makeCurrent();
    }
}

void EglGbmLayerSurface::destroyResources()
{
    m_surface = {};
    m_oldSurface = {};
}

static QList<FormatInfo> filterAndSortFormats(const QHash<uint32_t, QList<uint64_t>> &formats, EglGbmLayerSurface::FormatOption option, Output::ColorPowerTradeoff tradeoff)
{
    QList<FormatInfo> ret;
    for (auto it = formats.begin(); it != formats.end(); it++) {
        const auto info = FormatInfo::get(it.key());
        if (!info) {
            continue;
        }
        if (option == EglGbmLayerSurface::FormatOption::RequireAlpha && !info->alphaBits) {
            continue;
        }
        if (info->bitsPerColor < 8) {
            continue;
        }
        ret.push_back(*info);
    }
    std::ranges::sort(ret, [tradeoff](const FormatInfo &before, const FormatInfo &after) {
        if (tradeoff == Output::ColorPowerTradeoff::PreferAccuracy && before.bitsPerColor != after.bitsPerColor) {
            return before.bitsPerColor > after.bitsPerColor;
        }
        if (before.floatingPoint != after.floatingPoint) {
            return !before.floatingPoint;
        }
        const bool beforeHasAlpha = before.alphaBits != 0;
        const bool afterHasAlpha = after.alphaBits != 0;
        if (beforeHasAlpha != afterHasAlpha) {
            return beforeHasAlpha;
        }
        if (before.bitsPerPixel != after.bitsPerPixel) {
            return before.bitsPerPixel < after.bitsPerPixel;
        }
        return before.bitsPerColor > after.bitsPerColor;
    });
    return ret;
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayerSurface::startRendering(const QSize &bufferSize, OutputTransform transformation, const QHash<uint32_t, QList<uint64_t>> &formats, const ColorDescription &blendingColor, const ColorDescription &scanoutColor, const std::shared_ptr<IccProfile> &iccProfile, double scale, Output::ColorPowerTradeoff tradeoff, bool useShadowBuffer)
{
    if (!checkSurface(bufferSize, formats, tradeoff)) {
        return std::nullopt;
    }
    m_oldSurface.reset();

    if (!m_eglBackend->openglContext()->makeCurrent()) {
        return std::nullopt;
    }

    auto slot = m_surface->gbmSwapchain->acquire();
    if (!slot) {
        return std::nullopt;
    }

    if (slot->framebuffer()->colorAttachment()->contentTransform() != transformation) {
        m_surface->damageJournal.clear();
    }
    slot->framebuffer()->colorAttachment()->setContentTransform(transformation);
    m_surface->currentSlot = slot;
    m_surface->scale = scale;

    if (m_surface->blendingColor != blendingColor || m_surface->scanoutColor != scanoutColor || m_surface->iccProfile != iccProfile) {
        m_surface->damageJournal.clear();
        m_surface->shadowDamageJournal.clear();
        m_surface->needsShadowBuffer = useShadowBuffer;
        m_surface->blendingColor = blendingColor;
        m_surface->scanoutColor = scanoutColor;
        m_surface->iccProfile = iccProfile;
        if (iccProfile) {
            if (!m_surface->iccShader) {
                m_surface->iccShader = std::make_unique<IccShader>();
            }
        } else {
            m_surface->iccShader.reset();
        }
    }

    m_surface->compositingTimeQuery = std::make_unique<GLRenderTimeQuery>(m_surface->context);
    m_surface->compositingTimeQuery->begin();
    if (m_surface->needsShadowBuffer) {
        if (!m_surface->shadowSwapchain || m_surface->shadowSwapchain->size() != m_surface->gbmSwapchain->size()) {
            const auto formats = m_eglBackend->eglDisplayObject()->nonExternalOnlySupportedDrmFormats();
            const QList<FormatInfo> sortedFormats = filterAndSortFormats(formats, m_formatOption, tradeoff);
            for (const auto format : sortedFormats) {
                auto modifiers = formats[format.drmFormat];
                if (format.floatingPoint && m_eglBackend->gpu()->isAmdgpu() && qEnvironmentVariableIntValue("KWIN_DRM_NO_DCC_WORKAROUND") == 0) {
                    // using modifiers with DCC here causes glitches on amdgpu: https://gitlab.freedesktop.org/mesa/mesa/-/issues/10875
                    if (!modifiers.contains(DRM_FORMAT_MOD_LINEAR)) {
                        continue;
                    }
                    modifiers = {DRM_FORMAT_MOD_LINEAR};
                }
                m_surface->shadowSwapchain = EglSwapchain::create(m_eglBackend->drmDevice()->allocator(), m_eglBackend->openglContext(), m_surface->gbmSwapchain->size(), format.drmFormat, modifiers);
                if (m_surface->shadowSwapchain) {
                    break;
                }
            }
        }
        if (!m_surface->shadowSwapchain) {
            qCCritical(KWIN_DRM) << "Failed to create shadow swapchain!";
            return std::nullopt;
        }
        m_surface->currentShadowSlot = m_surface->shadowSwapchain->acquire();
        if (!m_surface->currentShadowSlot) {
            return std::nullopt;
        }
        m_surface->currentShadowSlot->texture()->setContentTransform(m_surface->currentSlot->framebuffer()->colorAttachment()->contentTransform());
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface->currentShadowSlot->framebuffer(), m_surface->blendingColor),
            .repaint = bufferAgeEnabled ? m_surface->shadowDamageJournal.accumulate(m_surface->currentShadowSlot->age(), infiniteRegion()) : infiniteRegion(),
        };
    } else {
        m_surface->shadowSwapchain.reset();
        m_surface->currentShadowSlot.reset();
        return OutputLayerBeginFrameInfo{
            .renderTarget = RenderTarget(m_surface->currentSlot->framebuffer(), m_surface->blendingColor),
            .repaint = bufferAgeEnabled ? m_surface->damageJournal.accumulate(slot->age(), infiniteRegion()) : infiniteRegion(),
        };
    }
}

static GLVertexBuffer *uploadGeometry(const QRegion &devicePixels, const QSize &fboSize)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));
    const auto optMap = vbo->map<GLVertex2D>(devicePixels.rectCount() * 6);
    if (!optMap) {
        return nullptr;
    }
    const auto map = *optMap;
    size_t vboIndex = 0;
    for (QRectF rect : devicePixels) {
        const float x0 = rect.left();
        const float y0 = rect.top();
        const float x1 = rect.right();
        const float y1 = rect.bottom();

        const float u0 = x0 / fboSize.width();
        const float v0 = y0 / fboSize.height();
        const float u1 = x1 / fboSize.width();
        const float v1 = y1 / fboSize.height();

        // first triangle
        map[vboIndex++] = GLVertex2D{
            .position = QVector2D(x0, y0),
            .texcoord = QVector2D(u0, v0),
        };
        map[vboIndex++] = GLVertex2D{
            .position = QVector2D(x1, y1),
            .texcoord = QVector2D(u1, v1),
        };
        map[vboIndex++] = GLVertex2D{
            .position = QVector2D(x0, y1),
            .texcoord = QVector2D(u0, v1),
        };

        // second triangle
        map[vboIndex++] = GLVertex2D{
            .position = QVector2D(x0, y0),
            .texcoord = QVector2D(u0, v0),
        };
        map[vboIndex++] = GLVertex2D{
            .position = QVector2D(x1, y0),
            .texcoord = QVector2D(u1, v0),
        };
        map[vboIndex++] = GLVertex2D{
            .position = QVector2D(x1, y1),
            .texcoord = QVector2D(u1, v1),
        };
    }
    vbo->unmap();
    vbo->setVertexCount(vboIndex);
    return vbo;
}

bool EglGbmLayerSurface::endRendering(const QRegion &damagedRegion, OutputFrame *frame)
{
    if (m_surface->needsShadowBuffer) {
        const QRegion logicalRepaint = damagedRegion | m_surface->damageJournal.accumulate(m_surface->currentSlot->age(), infiniteRegion());
        m_surface->damageJournal.add(damagedRegion);
        m_surface->shadowDamageJournal.add(damagedRegion);
        QRegion repaint;
        if (logicalRepaint == infiniteRegion()) {
            repaint = QRect(QPoint(), m_surface->gbmSwapchain->size());
        } else {
            const auto mapping = m_surface->currentShadowSlot->framebuffer()->colorAttachment()->contentTransform().combine(OutputTransform::FlipY);
            const QSize rotatedSize = mapping.map(m_surface->gbmSwapchain->size());
            for (const QRect rect : logicalRepaint) {
                repaint |= mapping.map(scaledRect(rect, m_surface->scale), rotatedSize).toAlignedRect() & QRect(QPoint(), m_surface->gbmSwapchain->size());
            }
        }

        GLFramebuffer *fbo = m_surface->currentSlot->framebuffer();
        GLFramebuffer::pushFramebuffer(fbo);
        ShaderBinder binder = m_surface->iccShader ? ShaderBinder(m_surface->iccShader->shader()) : ShaderBinder(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
        // this transform is absolute colorimetric, whitepoint adjustment is done in compositing already
        if (m_surface->iccShader) {
            m_surface->iccShader->setUniforms(m_surface->iccProfile, m_surface->blendingColor, RenderingIntent::AbsoluteColorimetric);
        } else {
            binder.shader()->setColorspaceUniforms(m_surface->blendingColor, m_surface->scanoutColor, RenderingIntent::AbsoluteColorimetric);
        }
        QMatrix4x4 mat;
        mat.scale(1, -1);
        mat.ortho(QRectF(QPointF(), fbo->size()));
        binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mat);
        glDisable(GL_BLEND);
        if (const auto vbo = uploadGeometry(repaint, m_surface->gbmSwapchain->size())) {
            m_surface->currentShadowSlot->texture()->bind();
            vbo->render(GL_TRIANGLES);
            m_surface->currentShadowSlot->texture()->unbind();
        }
        EGLNativeFence fence(m_surface->context->displayObject());
        m_surface->shadowSwapchain->release(m_surface->currentShadowSlot, fence.takeFileDescriptor());
        GLFramebuffer::popFramebuffer();
    } else {
        m_surface->damageJournal.add(damagedRegion);
    }
    m_surface->compositingTimeQuery->end();
    if (frame) {
        frame->addRenderTimeQuery(std::move(m_surface->compositingTimeQuery));
    }
    glFlush();
    EGLNativeFence sourceFence(m_eglBackend->eglDisplayObject());
    if (!sourceFence.isValid()) {
        // llvmpipe doesn't do synchronization properly: https://gitlab.freedesktop.org/mesa/mesa/-/issues/9375
        // and NVidia doesn't support implicit sync
        glFinish();
    }
    m_surface->gbmSwapchain->release(m_surface->currentSlot, sourceFence.fileDescriptor().duplicate());
    const auto buffer = importBuffer(m_surface.get(), m_surface->currentSlot.get(), sourceFence.takeFileDescriptor(), frame, damagedRegion);
    if (buffer) {
        m_surface->currentFramebuffer = buffer;
        return true;
    } else {
        return false;
    }
}

EglGbmBackend *EglGbmLayerSurface::eglBackend() const
{
    return m_eglBackend;
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::currentBuffer() const
{
    return m_surface ? m_surface->currentFramebuffer : nullptr;
}

const ColorDescription &EglGbmLayerSurface::colorDescription() const
{
    if (m_surface) {
        return m_surface->blendingColor;
    } else {
        return ColorDescription::sRGB;
    }
}

std::shared_ptr<GLTexture> EglGbmLayerSurface::texture() const
{
    if (m_surface) {
        return m_surface->currentShadowSlot ? m_surface->currentShadowSlot->texture() : m_surface->currentSlot->texture();
    } else {
        return nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::renderTestBuffer(const QSize &bufferSize, const QHash<uint32_t, QList<uint64_t>> &formats, Output::ColorPowerTradeoff tradeoff)
{
    EglContext *context = m_eglBackend->openglContext();
    if (!context->makeCurrent()) {
        qCWarning(KWIN_DRM) << "EglGbmLayerSurface::renderTestBuffer: failed to make opengl context current";
        return nullptr;
    }

    if (checkSurface(bufferSize, formats, tradeoff)) {
        return m_surface->currentFramebuffer;
    } else {
        return nullptr;
    }
}

void EglGbmLayerSurface::forgetDamage()
{
    if (m_surface) {
        m_surface->damageJournal.clear();
        m_surface->importDamageJournal.clear();
        m_surface->shadowDamageJournal.clear();
    }
}

bool EglGbmLayerSurface::checkSurface(const QSize &size, const QHash<uint32_t, QList<uint64_t>> &formats, Output::ColorPowerTradeoff tradeoff)
{
    if (doesSurfaceFit(m_surface.get(), size, formats, tradeoff)) {
        return true;
    }
    if (doesSurfaceFit(m_oldSurface.get(), size, formats, tradeoff)) {
        m_surface = std::move(m_oldSurface);
        return true;
    }
    if (auto newSurface = createSurface(size, formats, tradeoff)) {
        m_oldSurface = std::move(m_surface);
        if (m_oldSurface) {
            // FIXME: Use absolute frame sequence numbers for indexing the DamageJournal
            m_oldSurface->damageJournal.clear();
            m_oldSurface->shadowDamageJournal.clear();
            m_oldSurface->gbmSwapchain->resetBufferAge();
            if (m_oldSurface->shadowSwapchain) {
                m_oldSurface->shadowSwapchain->resetBufferAge();
            }
            if (m_oldSurface->importGbmSwapchain) {
                m_oldSurface->importGbmSwapchain->resetBufferAge();
                m_oldSurface->importDamageJournal.clear();
            }
        }
        m_surface = std::move(newSurface);
        return true;
    }
    return false;
}

bool EglGbmLayerSurface::doesSurfaceFit(Surface *surface, const QSize &size, const QHash<uint32_t, QList<uint64_t>> &formats, Output::ColorPowerTradeoff tradeoff) const
{
    if (!surface || surface->needsRecreation || !surface->gbmSwapchain || surface->gbmSwapchain->size() != size) {
        return false;
    }
    if (surface->tradeoff != tradeoff) {
        return false;
    }
    if (surface->bufferTarget == BufferTarget::Dumb) {
        return formats.contains(surface->importDumbSwapchain->format());
    }
    switch (surface->importMode) {
    case MultiGpuImportMode::None:
    case MultiGpuImportMode::Dmabuf:
    case MultiGpuImportMode::LinearDmabuf: {
        const auto format = surface->gbmSwapchain->format();
        return formats.contains(format) && (surface->gbmSwapchain->modifier() == DRM_FORMAT_MOD_INVALID || formats[format].contains(surface->gbmSwapchain->modifier()));
    }
    case MultiGpuImportMode::DumbBuffer:
        return formats.contains(surface->importDumbSwapchain->format());
    case MultiGpuImportMode::Egl: {
        const auto format = surface->importGbmSwapchain->format();
        const auto it = formats.find(format);
        return it != formats.end() && (surface->importGbmSwapchain->modifier() == DRM_FORMAT_MOD_INVALID || it->contains(surface->importGbmSwapchain->modifier()));
    }
    }
    Q_UNREACHABLE();
}

std::unique_ptr<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, const QHash<uint32_t, QList<uint64_t>> &formats, Output::ColorPowerTradeoff tradeoff) const
{
    const QList<FormatInfo> sortedFormats = filterAndSortFormats(formats, m_formatOption, tradeoff);

    // special case: the cursor plane needs linear, but not all GPUs (NVidia) can render to linear
    auto bufferTarget = m_requestedBufferTarget;
    if (m_gpu == m_eglBackend->gpu()) {
        const bool needsLinear = std::ranges::all_of(sortedFormats, [&formats](const FormatInfo &fmt) {
            const auto &mods = formats[fmt.drmFormat];
            return std::ranges::all_of(mods, [](uint64_t mod) {
                return mod == DRM_FORMAT_MOD_LINEAR;
            });
        });
        if (needsLinear) {
            const auto renderFormats = m_eglBackend->eglDisplayObject()->allSupportedDrmFormats();
            const bool noLinearSupport = std::ranges::none_of(sortedFormats, [&renderFormats](const auto &formatInfo) {
                const auto it = renderFormats.constFind(formatInfo.drmFormat);
                return it != renderFormats.cend() && it->nonExternalOnlyModifiers.contains(DRM_FORMAT_MOD_LINEAR);
            });
            if (noLinearSupport) {
                bufferTarget = BufferTarget::Dumb;
            }
        }
    }

    const auto doTestFormats = [this, &size, &formats, bufferTarget, tradeoff](const QList<FormatInfo> &gbmFormats, MultiGpuImportMode importMode) -> std::unique_ptr<Surface> {
        for (const auto &format : gbmFormats) {
            auto surface = createSurface(size, format.drmFormat, formats[format.drmFormat], importMode, bufferTarget, tradeoff);
            if (surface) {
                return surface;
            }
        }
        return nullptr;
    };
    if (m_gpu == m_eglBackend->gpu()) {
        return doTestFormats(sortedFormats, MultiGpuImportMode::None);
    }
    // special case, we're using different display devices but the same render device
    const auto display = m_eglBackend->displayForGpu(m_gpu);
    if (display && !display->renderNode().isEmpty() && display->renderNode() == m_eglBackend->eglDisplayObject()->renderNode()) {
        if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::None)) {
            return surface;
        }
    }
    if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::Egl)) {
        qCDebug(KWIN_DRM) << "chose egl import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
        return surface;
    }
    if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::Dmabuf)) {
        qCDebug(KWIN_DRM) << "chose dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
        return surface;
    }
    if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::LinearDmabuf)) {
        qCDebug(KWIN_DRM) << "chose linear dmabuf import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
        return surface;
    }
    if (auto surface = doTestFormats(sortedFormats, MultiGpuImportMode::DumbBuffer)) {
        qCDebug(KWIN_DRM) << "chose cpu import with format" << formatName(surface->gbmSwapchain->format()).name << "and modifier" << surface->gbmSwapchain->modifier();
        return surface;
    }
    return nullptr;
}

static QList<uint64_t> filterModifiers(const QList<uint64_t> &one, const QList<uint64_t> &two)
{
    QList<uint64_t> ret = one;
    ret.erase(std::remove_if(ret.begin(), ret.end(), [&two](uint64_t mod) {
                  return !two.contains(mod);
              }),
              ret.end());
    return ret;
}

std::unique_ptr<EglGbmLayerSurface::Surface> EglGbmLayerSurface::createSurface(const QSize &size, uint32_t format, const QList<uint64_t> &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget, Output::ColorPowerTradeoff tradeoff) const
{
    const bool cpuCopy = importMode == MultiGpuImportMode::DumbBuffer || bufferTarget == BufferTarget::Dumb;
    QList<uint64_t> renderModifiers;
    auto ret = std::make_unique<Surface>();
    const auto drmFormat = m_eglBackend->eglDisplayObject()->allSupportedDrmFormats()[format];
    if (importMode == MultiGpuImportMode::Egl) {
        ret->importContext = m_eglBackend->contextForGpu(m_gpu);
        if (!ret->importContext || ret->importContext->isSoftwareRenderer()) {
            return nullptr;
        }
        const auto importDrmFormat = ret->importContext->displayObject()->allSupportedDrmFormats()[format];
        renderModifiers = filterModifiers(importDrmFormat.allModifiers,
                                          drmFormat.nonExternalOnlyModifiers);
        // transferring non-linear buffers with implicit modifiers between GPUs is likely to yield wrong results
        renderModifiers.removeAll(DRM_FORMAT_MOD_INVALID);
    } else if (cpuCopy) {
        if (!cpuCopyFormats.contains(format)) {
            return nullptr;
        }
        renderModifiers = drmFormat.nonExternalOnlyModifiers;
    } else {
        renderModifiers = filterModifiers(modifiers, drmFormat.nonExternalOnlyModifiers);
    }
    if (renderModifiers.empty()) {
        return nullptr;
    }
    ret->context = m_eglBackend->contextForGpu(m_eglBackend->gpu());
    ret->bufferTarget = bufferTarget;
    ret->importMode = importMode;
    ret->gbmSwapchain = createGbmSwapchain(m_eglBackend->gpu(), m_eglBackend->openglContext(), size, format, renderModifiers, importMode, bufferTarget);
    ret->tradeoff = tradeoff;
    if (!ret->gbmSwapchain) {
        return nullptr;
    }
    if (cpuCopy) {
        ret->importDumbSwapchain = std::make_unique<QPainterSwapchain>(m_gpu->drmDevice()->allocator(), size, format);
    } else if (importMode == MultiGpuImportMode::Egl) {
        ret->importGbmSwapchain = createGbmSwapchain(m_gpu, ret->importContext.get(), size, format, modifiers, MultiGpuImportMode::None, BufferTarget::Normal);
        if (!ret->importGbmSwapchain) {
            return nullptr;
        }
    }
    if (!doRenderTestBuffer(ret.get())) {
        return nullptr;
    }
    return ret;
}

std::shared_ptr<EglSwapchain> EglGbmLayerSurface::createGbmSwapchain(DrmGpu *gpu, EglContext *context, const QSize &size, uint32_t format, const QList<uint64_t> &modifiers, MultiGpuImportMode importMode, BufferTarget bufferTarget) const
{
    static bool modifiersEnvSet = false;
    static const bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    bool allowModifiers = (m_gpu->addFB2ModifiersSupported() || importMode == MultiGpuImportMode::Egl || importMode == MultiGpuImportMode::DumbBuffer) && (!modifiersEnvSet || (modifiersEnvSet && modifiersEnv)) && modifiers != implicitModifier;
#if !HAVE_GBM_BO_GET_FD_FOR_PLANE
    allowModifiers &= m_gpu == gpu;
#endif
    const bool linearSupported = modifiers.contains(DRM_FORMAT_MOD_LINEAR);
    const bool preferLinear = importMode == MultiGpuImportMode::DumbBuffer || bufferTarget == BufferTarget::Linear;
    const bool forceLinear = importMode == MultiGpuImportMode::LinearDmabuf || (importMode != MultiGpuImportMode::None && importMode != MultiGpuImportMode::DumbBuffer && !allowModifiers);
    if (forceLinear && !linearSupported) {
        return nullptr;
    }
    if (linearSupported && (preferLinear || forceLinear)) {
        if (const auto swapchain = EglSwapchain::create(gpu->drmDevice()->allocator(), context, size, format, linearModifier)) {
            return swapchain;
        } else if (forceLinear) {
            return nullptr;
        }
    }

    if (allowModifiers) {
        if (auto swapchain = EglSwapchain::create(gpu->drmDevice()->allocator(), context, size, format, modifiers)) {
            return swapchain;
        }
    }

    return EglSwapchain::create(gpu->drmDevice()->allocator(), context, size, format, implicitModifier);
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::doRenderTestBuffer(Surface *surface) const
{
    auto slot = surface->gbmSwapchain->acquire();
    if (!slot) {
        return nullptr;
    }
    if (!m_gpu->atomicModeSetting()) {
        EglContext::currentContext()->pushFramebuffer(slot->framebuffer());
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        EglContext::currentContext()->popFramebuffer();
    }
    if (const auto ret = importBuffer(surface, slot.get(), FileDescriptor{}, nullptr, infiniteRegion())) {
        surface->currentSlot = slot;
        surface->currentFramebuffer = ret;
        return ret;
    } else {
        return nullptr;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importBuffer(Surface *surface, EglSwapchainSlot *slot, FileDescriptor &&readFence, OutputFrame *frame, const QRegion &damagedRegion) const
{
    if (surface->bufferTarget == BufferTarget::Dumb || surface->importMode == MultiGpuImportMode::DumbBuffer) {
        return importWithCpu(surface, slot, frame);
    } else if (surface->importMode == MultiGpuImportMode::Egl) {
        return importWithEgl(surface, slot->buffer(), std::move(readFence), frame, damagedRegion);
    } else {
        const auto ret = m_gpu->importBuffer(slot->buffer(), std::move(readFence));
        if (!ret) {
            qCWarning(KWIN_DRM, "Failed to create framebuffer: %s", strerror(errno));
        }
        return ret;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithEgl(Surface *surface, GraphicsBuffer *sourceBuffer, FileDescriptor &&readFence, OutputFrame *frame, const QRegion &damagedRegion) const
{
    Q_ASSERT(surface->importGbmSwapchain);

    const auto display = m_eglBackend->displayForGpu(m_gpu);
    // older versions of the NVidia proprietary driver support neither implicit sync nor EGL_ANDROID_native_fence_sync
    if (!readFence.isValid() || !display->supportsNativeFence()) {
        glFinish();
    }

    if (!surface->importContext->makeCurrent()) {
        qCWarning(KWIN_DRM, "Failed to make import context current");
        // this is probably caused by a GPU reset, let's not take any chances
        surface->needsRecreation = true;
        m_eglBackend->resetContextForGpu(m_gpu);
        return nullptr;
    }
    const auto restoreContext = qScopeGuard([this]() {
        m_eglBackend->openglContext()->makeCurrent();
    });
    if (surface->importContext->checkGraphicsResetStatus() != GL_NO_ERROR) {
        qCWarning(KWIN_DRM, "Detected GPU reset on secondary GPU %s", qPrintable(m_gpu->drmDevice()->path()));
        surface->needsRecreation = true;
        m_eglBackend->resetContextForGpu(m_gpu);
        return nullptr;
    }
    std::unique_ptr<GLRenderTimeQuery> renderTime;
    if (frame) {
        renderTime = std::make_unique<GLRenderTimeQuery>(surface->importContext);
        renderTime->begin();
    }

    if (readFence.isValid()) {
        const auto destinationFence = EGLNativeFence::importFence(surface->importContext->displayObject(), std::move(readFence));
        destinationFence.waitSync();
    }

    auto &sourceTexture = surface->importedTextureCache[sourceBuffer];
    if (!sourceTexture) {
        sourceTexture = surface->importContext->importDmaBufAsTexture(*sourceBuffer->dmabufAttributes());
    }
    if (!sourceTexture) {
        qCWarning(KWIN_DRM, "failed to import the source texture!");
        return nullptr;
    }
    auto slot = surface->importGbmSwapchain->acquire();
    if (!slot) {
        qCWarning(KWIN_DRM, "failed to import the local texture!");
        return nullptr;
    }

    QRegion deviceDamage;
    if (damagedRegion == infiniteRegion()) {
        deviceDamage = QRect(QPoint(), surface->gbmSwapchain->size());
    } else {
        const auto mapping = surface->currentSlot->framebuffer()->colorAttachment()->contentTransform().combine(OutputTransform::FlipY);
        const QSize rotatedSize = mapping.map(surface->gbmSwapchain->size());
        for (const QRect rect : damagedRegion) {
            deviceDamage |= mapping.map(scaledRect(rect, surface->scale), rotatedSize).toAlignedRect() & QRect(QPoint(), surface->gbmSwapchain->size());
        }
    }
    const QRegion repaint = (deviceDamage | surface->importDamageJournal.accumulate(slot->age(), infiniteRegion())) & QRect(QPoint(), surface->gbmSwapchain->size());
    surface->importDamageJournal.add(deviceDamage);

    GLFramebuffer *fbo = slot->framebuffer();
    surface->importContext->pushFramebuffer(fbo);

    const auto shader = surface->importContext->shaderManager()->pushShader(sourceTexture->target() == GL_TEXTURE_EXTERNAL_OES ? ShaderTrait::MapExternalTexture : ShaderTrait::MapTexture);
    QMatrix4x4 mat;
    mat.scale(1, -1);
    mat.ortho(QRect(QPoint(), fbo->size()));
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mat);

    if (const auto vbo = uploadGeometry(repaint, fbo->size())) {
        sourceTexture->bind();
        vbo->render(GL_TRIANGLES);
        sourceTexture->unbind();
    }

    surface->importContext->popFramebuffer();
    surface->importContext->shaderManager()->popShader();
    glFlush();
    EGLNativeFence endFence(display);
    if (!endFence.isValid()) {
        glFinish();
    }
    surface->importGbmSwapchain->release(slot, endFence.fileDescriptor().duplicate());
    if (frame) {
        renderTime->end();
        frame->addRenderTimeQuery(std::move(renderTime));
    }

    return m_gpu->importBuffer(slot->buffer(), endFence.takeFileDescriptor());
}

std::shared_ptr<DrmFramebuffer> EglGbmLayerSurface::importWithCpu(Surface *surface, EglSwapchainSlot *source, OutputFrame *frame) const
{
    std::unique_ptr<CpuRenderTimeQuery> copyTime;
    if (frame) {
        copyTime = std::make_unique<CpuRenderTimeQuery>();
    }
    Q_ASSERT(surface->importDumbSwapchain);
    const auto slot = surface->importDumbSwapchain->acquire();
    if (!slot) {
        qCWarning(KWIN_DRM) << "EglGbmLayerSurface::importWithCpu: failed to get a target dumb buffer";
        return nullptr;
    }
    const auto size = source->buffer()->size();
    const qsizetype srcStride = 4 * size.width();
    EglContext *context = m_eglBackend->openglContext();
    GLFramebuffer::pushFramebuffer(source->framebuffer());
    QImage *const dst = slot->view()->image();
    if (dst->bytesPerLine() == srcStride) {
        context->glReadnPixels(0, 0, dst->width(), dst->height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, dst->sizeInBytes(), dst->bits());
    } else {
        // there's padding, need to copy line by line
        if (surface->cpuCopyCache.size() != dst->size()) {
            surface->cpuCopyCache = QImage(dst->size(), QImage::Format_RGBA8888);
        }
        context->glReadnPixels(0, 0, dst->width(), dst->height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface->cpuCopyCache.sizeInBytes(), surface->cpuCopyCache.bits());
        for (int i = 0; i < dst->height(); i++) {
            std::memcpy(dst->scanLine(i), surface->cpuCopyCache.scanLine(i), srcStride);
        }
    }
    GLFramebuffer::popFramebuffer();

    const auto ret = m_gpu->importBuffer(slot->buffer(), FileDescriptor{});
    if (!ret) {
        qCWarning(KWIN_DRM, "Failed to create a framebuffer: %s", strerror(errno));
    }
    surface->importDumbSwapchain->release(slot);
    if (frame) {
        copyTime->end();
        frame->addRenderTimeQuery(std::move(copyTime));
    }
    return ret;
}
}

#include "moc_drm_egl_layer_surface.cpp"

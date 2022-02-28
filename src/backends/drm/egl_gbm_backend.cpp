/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_gbm_backend.h"
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
// kwin
#include "composite.h"
#include "drm_backend.h"
#include "drm_buffer_gbm.h"
#include "drm_output.h"
#include "gbm_surface.h"
#include "logging.h"
#include "options.h"
#include "renderloop_p.h"
#include "screens.h"
#include "surfaceitem_wayland.h"
#include "drm_gpu.h"
#include "linux_dmabuf.h"
#include "dumb_swapchain.h"
#include "kwineglutils_p.h"
#include "shadowbuffer.h"
#include "drm_pipeline.h"
#include "drm_abstract_output.h"
#include "egl_dmabuf.h"
// kwin libs
#include <kwinglplatform.h>
#include <kwineglimagetexture.h>
// system
#include <gbm.h>
#include <unistd.h>
#include <errno.h>
#include <drm_fourcc.h>
// kwayland server
#include "KWaylandServer/surface_interface.h"
#include "KWaylandServer/linuxdmabufv1clientbuffer.h"
#include "KWaylandServer/clientconnection.h"

namespace KWin
{

EglGbmBackend::EglGbmBackend(DrmBackend *drmBackend, DrmGpu *gpu)
    : AbstractEglBackend(gpu->deviceId())
    , m_backend(drmBackend)
    , m_gpu(gpu)
{
    m_gpu->setEglBackend(this);
    connect(m_gpu, &DrmGpu::outputAdded, this, &EglGbmBackend::addOutput);
    connect(m_gpu, &DrmGpu::outputRemoved, this, &EglGbmBackend::removeOutput);
    setIsDirectRendering(true);
}

EglGbmBackend::~EglGbmBackend()
{
    cleanup();
}

void EglGbmBackend::cleanupSurfaces()
{
    // shadow buffer needs context current for destruction
    makeCurrent();
    m_outputs.clear();
}

void EglGbmBackend::cleanupRenderData(Output::RenderData &render)
{
    render.gbmSurface = nullptr;
    render.importSwapchain = nullptr;
    if (render.shadowBuffer) {
        makeContextCurrent(render);
        render.shadowBuffer = nullptr;
    }
}

bool EglGbmBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_gpu->eglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        const bool hasMesaGBM = hasClientExtension(QByteArrayLiteral("EGL_MESA_platform_gbm"));
        const bool hasKHRGBM = hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_gbm"));
        const GLenum platform = hasMesaGBM ? EGL_PLATFORM_GBM_MESA : EGL_PLATFORM_GBM_KHR;

        if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base")) ||
                (!hasMesaGBM && !hasKHRGBM)) {
            setFailed("Missing one or more extensions between EGL_EXT_platform_base, "
                      "EGL_MESA_platform_gbm, EGL_KHR_platform_gbm");
            return false;
        }

        if (!m_gpu->gbmDevice()) {
            setFailed("Could not create gbm device");
            return false;
        }

        display = eglGetPlatformDisplayEXT(platform, m_gpu->gbmDevice(), nullptr);
        m_gpu->setEglDisplay(display);
    }

    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    setEglDisplay(display);
    return initEglAPI();
}

void EglGbmBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }

    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }
    initBufferAge();
    // at the moment: no secondary GPU -> no OpenGL context!
    if (isPrimary()) {
        initKWinGL();
        initWayland();
    }
}

bool EglGbmBackend::initRenderingContext()
{
    if (!initBufferConfigs()) {
        return false;
    }
    if (isPrimary()) {
        if (!createContext() || !makeCurrent()) {
            return false;
        }
    }
    const auto outputs = m_gpu->outputs();
    for (const auto &output : outputs) {
        addOutput(output);
    }
    return true;
}

bool EglGbmBackend::resetOutput(Output &output)
{
    std::optional<GbmFormat> gbmFormat = chooseFormat(output);
    if (!gbmFormat.has_value()) {
        qCCritical(KWIN_DRM) << "Could not find a suitable format for output" << output.output;
        return false;
    }
    uint32_t format = gbmFormat.value().drmFormat;
    QVector<uint64_t> modifiers = output.output->supportedModifiers(format);
    const QSize size = output.output->bufferSize();

    QSharedPointer<GbmSurface> gbmSurface;
    bool modifiersEnvSet = false;
    static bool modifiersEnv = qEnvironmentVariableIntValue("KWIN_DRM_USE_MODIFIERS", &modifiersEnvSet) != 0;
    static bool allowModifiers = gpu()->addFB2ModifiersSupported() && ((gpu()->isNVidia() && !modifiersEnvSet) || (modifiersEnvSet && modifiersEnv));
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    if (!allowModifiers) {
#else
    // modifiers have to be disabled with multi-gpu if gbm_bo_get_fd_for_plane is not available
    if (!allowModifiers || output.output->gpu() != m_gpu) {
#endif
        int flags = GBM_BO_USE_RENDERING;
        if (output.output->gpu() == m_gpu) {
            flags |= GBM_BO_USE_SCANOUT;
        } else {
            flags |= GBM_BO_USE_LINEAR;
        }
        gbmSurface = QSharedPointer<GbmSurface>::create(m_gpu, size, format, flags, m_configs[format]);
    } else {
        gbmSurface = QSharedPointer<GbmSurface>::create(m_gpu, size, format, modifiers, m_configs[format]);
        if (!gbmSurface->isValid()) {
            // the egl / gbm implementation may reject the modifier list from another gpu
            // as a fallback use linear, to at least make CPU copy more efficient
            modifiers = {DRM_FORMAT_MOD_LINEAR};
            gbmSurface = QSharedPointer<GbmSurface>::create(m_gpu, size, format, modifiers, m_configs[format]);
        }
    }
    if (!gbmSurface->isValid()) {
        qCCritical(KWIN_DRM) << "Creating GBM surface failed:" << strerror(errno);
        return false;
    }
    cleanupRenderData(output.old);
    output.old = output.current;
    output.current = {};
    output.current.format = gbmFormat.value();
    output.current.gbmSurface = gbmSurface;

    if (!output.output->needsSoftwareTransformation())  {
        output.current.shadowBuffer = nullptr;
    } else {
        makeContextCurrent(output.current);
        output.current.shadowBuffer = QSharedPointer<ShadowBuffer>::create(output.output->sourceSize(), output.current.format);
        if (!output.current.shadowBuffer->isComplete()) {
            return false;
        }
    }
    return true;
}

bool EglGbmBackend::addOutput(AbstractOutput *output)
{
    const auto drmOutput = static_cast<DrmAbstractOutput*>(output);
    Output newOutput;
    newOutput.output = drmOutput;
    if (!isPrimary() && !renderingBackend()->addOutput(drmOutput)) {
        return false;
    }
    m_outputs.insert(drmOutput, newOutput);
    return true;
}

void EglGbmBackend::removeOutput(AbstractOutput *output)
{
    Q_ASSERT(m_outputs.contains(output));
    if (isPrimary()) {
        // shadow buffer needs context current for destruction
        makeCurrent();
    } else {
        renderingBackend()->removeOutput(output);
    }
    m_outputs.remove(output);
}

bool EglGbmBackend::swapBuffers(DrmAbstractOutput *drmOutput, const QRegion &dirty)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    Output &output = m_outputs[drmOutput];
    renderFramebufferToSurface(output);
    if (output.current.gbmSurface->swapBuffers()) {
        cleanupRenderData(output.old);
        updateBufferAge(output, dirty);
        return true;
    } else {
        return false;
    }
}

bool EglGbmBackend::exportFramebuffer(DrmAbstractOutput *drmOutput, void *data, const QSize &size, uint32_t stride)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    auto bo = m_outputs[drmOutput].current.gbmSurface->currentBuffer();
    if (!bo->map(GBM_BO_TRANSFER_READ)) {
        return false;
    }
    if (stride != bo->stride()) {
        // shouldn't happen if formats are the same
        return false;
    }
    return memcpy(data, bo->mappedData(), size.height() * stride);
}

bool EglGbmBackend::exportFramebufferAsDmabuf(DrmAbstractOutput *drmOutput, int *fds, int *strides, int *offsets, uint32_t *num_fds, uint32_t *format, uint64_t *modifier)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    auto bo = m_outputs[drmOutput].current.gbmSurface->currentBuffer()->getBo();
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    if (gbm_bo_get_handle_for_plane(bo, 0).s32 != -1) {
        *num_fds = gbm_bo_get_plane_count(bo);
        for (uint32_t i = 0; i < *num_fds; i++) {
            fds[i] = gbm_bo_get_fd_for_plane(bo, i);
            if (fds[i] < 0) {
                qCWarning(KWIN_DRM) << "failed to export gbm_bo as dma-buf:" << strerror(errno);
                for (uint32_t f = 0; f < i; f++) {
                    close(fds[f]);
                }
                return false;
            }
            strides[i] = gbm_bo_get_stride_for_plane(bo, i);
            offsets[i] = gbm_bo_get_offset(bo, i);
        }
        *modifier = gbm_bo_get_modifier(bo);
    } else {
#endif
        fds[0] = gbm_bo_get_fd(bo);
        if (fds[0] < 0) {
            qCWarning(KWIN_DRM) << "failed to export gbm_bo as dma-buf:" << strerror(errno);
            return false;
        }
        *num_fds = 1;
        strides[0] = gbm_bo_get_stride(bo);
        *modifier = DRM_FORMAT_MOD_INVALID;
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    }
#endif
    *format = gbm_bo_get_format(bo);
    return true;
}

QSharedPointer<DrmBuffer> EglGbmBackend::importFramebuffer(Output &output, const QRegion &dirty) const
{
    if (!renderingBackend()->swapBuffers(output.output, dirty)) {
        qCWarning(KWIN_DRM) << "swapping buffers failed on output" << output.output;
        return nullptr;
    }
    const auto size = output.output->modeSize();
    if (output.current.importMode == ImportMode::Dmabuf) {
        struct gbm_import_fd_modifier_data data;
        data.width = size.width();
        data.height = size.height();
        if (renderingBackend()->exportFramebufferAsDmabuf(output.output, data.fds, data.strides, data.offsets, &data.num_fds, &data.format, &data.modifier)) {
            gbm_bo *importedBuffer = nullptr;
            if (data.modifier == DRM_FORMAT_MOD_INVALID) {
                struct gbm_import_fd_data data1;
                data1.fd = data.fds[0];
                data1.width = size.width();
                data1.height = size.height();
                data1.stride = data.strides[0];
                data1.format = data.format;
                importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD, &data1, GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
            } else {
                importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, 0);
            }
            for (uint32_t i = 0; i < data.num_fds; i++) {
                close(data.fds[i]);
            }
            if (importedBuffer) {
                auto buffer = QSharedPointer<DrmGbmBuffer>::create(m_gpu, importedBuffer, nullptr);
                if (buffer->bufferId() > 0) {
                    return buffer;
                }
            }
        }
        qCDebug(KWIN_DRM) << "import with dmabuf failed! Switching to CPU import on output" << output.output;
        output.current.importMode = ImportMode::DumbBuffer;
    }
    // ImportMode::DumbBuffer
    if (!output.current.importSwapchain || output.current.importSwapchain->size() != size) {
        const uint32_t format = renderingBackend()->drmFormat(output.output);
        output.current.importSwapchain = QSharedPointer<DumbSwapchain>::create(m_gpu, size, format);
        if (output.current.importSwapchain->isEmpty() || output.current.importSwapchain->currentBuffer()->format() != format) {
            output.current.importSwapchain = nullptr;
        }
    }
    if (output.current.importSwapchain) {
        auto buffer = output.current.importSwapchain->acquireBuffer();
        if (renderingBackend()->exportFramebuffer(output.output, buffer->data(), size, buffer->stride())) {
            return buffer;
        }
    }
    qCWarning(KWIN_DRM) << "all imports failed on output" << output.output;
    // try again with XRGB8888, the most universally supported basic format
    output.forceXrgb8888 = true;
    renderingBackend()->setForceXrgb8888(output.output);
    return nullptr;
}

void EglGbmBackend::renderFramebufferToSurface(Output &output)
{
    if (!output.current.shadowBuffer) {
        // No additional render target.
        return;
    }
    makeContextCurrent(output.current);
    output.current.shadowBuffer->render(output.output);
}

bool EglGbmBackend::makeContextCurrent(const Output::RenderData &render) const
{
    Q_ASSERT(isPrimary());
    const auto surface = render.gbmSurface;
    if (!surface) {
        return false;
    }
    if (eglMakeCurrent(eglDisplay(), surface->eglSurface(), surface->eglSurface(), context()) == EGL_FALSE) {
        qCCritical(KWIN_DRM) << "eglMakeCurrent failed:" << getEglErrorString();
        return false;
    }
    if (!GLPlatform::instance()->isGLES()) {
        glDrawBuffer(GL_BACK);
    }
    return true;
}

bool EglGbmBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (!eglChooseConfig(eglDisplay(), config_attribs, configs,
                         sizeof(configs) / sizeof(EGLConfig),
                         &count)) {
        qCCritical(KWIN_DRM) << "eglChooseConfig failed:" << getEglErrorString();
        return false;
    }

    setConfig(EGL_NO_CONFIG_KHR);

    // Loop through all configs, choosing the first one that has suitable format.
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat;
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);

        GbmFormat format;
        format.drmFormat = gbmFormat;
        // Query number of bits for color channel
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &format.redSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &format.greenSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &format.blueSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &format.alphaSize);

        if (m_formats.contains(format)) {
            continue;
        }
        m_formats << format;
        m_configs[format.drmFormat] = configs[i];
    }

    QVector<int> colorDepthOrder = {30, 24};
    bool ok = false;
    const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &ok);
    if (ok) {
        colorDepthOrder.prepend(preferred);
    }

    std::sort(m_formats.begin(), m_formats.end(), [&colorDepthOrder](const auto &lhs, const auto &rhs) {
        const int ls = lhs.redSize + lhs.greenSize + lhs.blueSize;
        const int rs = rhs.redSize + rhs.greenSize + rhs.blueSize;
        if (ls == rs) {
            return lhs.alphaSize < rhs.alphaSize;
        } else {
            for (const int &d : qAsConst(colorDepthOrder)) {
                if (ls == d) {
                    return true;
                } else if (rs == d) {
                    return false;
                }
            }
            return ls > rs;
        }
    });
    if (!m_formats.isEmpty()) {
        return true;
    }

    qCCritical(KWIN_DRM, "Choosing EGL config did not return a supported config. There were %u configs", count);
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat, blueSize, redSize, greenSize, alphaSize;
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &redSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &greenSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &blueSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &alphaSize);
        gbm_format_name_desc name;
        gbm_format_get_name(gbmFormat, &name);
        qCCritical(KWIN_DRM, "EGL config %d has format %s with %d,%d,%d,%d bits for r,g,b,a",  i, name.name, redSize, greenSize, blueSize, alphaSize);
    }
    return false;
}

static QVector<EGLint> regionToRects(const QRegion &region, AbstractWaylandOutput *output)
{
    const int height = output->modeSize().height();

    const QMatrix4x4 matrix = DrmOutput::logicalToNativeMatrix(output->geometry(),
                                                               output->scale(),
                                                               output->transform());

    QVector<EGLint> rects;
    rects.reserve(region.rectCount() * 4);
    for (const QRect &_rect : region) {
        const QRect rect = matrix.mapRect(_rect);

        rects << rect.left();
        rects << height - (rect.y() + rect.height());
        rects << rect.width();
        rects << rect.height();
    }
    return rects;
}

void EglGbmBackend::aboutToStartPainting(AbstractOutput *drmOutput, const QRegion &damagedRegion)
{
    Q_ASSERT_X(drmOutput, "aboutToStartPainting", "not using per screen rendering");
    Q_ASSERT(m_outputs.contains(drmOutput));
    const Output &output = m_outputs[drmOutput];
    if (output.current.bufferAge > 0 && !damagedRegion.isEmpty() && supportsPartialUpdate()) {
        const QRegion region = damagedRegion & output.output->geometry();

        QVector<EGLint> rects = regionToRects(region, output.output);
        const bool correct = eglSetDamageRegionKHR(eglDisplay(), output.current.gbmSurface->eglSurface(),
                                                   rects.data(), rects.count()/4);
        if (!correct) {
            qCWarning(KWIN_DRM) << "eglSetDamageRegionKHR failed:" << getEglErrorString();
        }
    }
}

SurfaceTexture *EglGbmBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return new BasicEGLSurfaceTextureInternal(this, pixmap);
}

SurfaceTexture *EglGbmBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return new BasicEGLSurfaceTextureWayland(this, pixmap);
}

void EglGbmBackend::setViewport(const Output &output) const
{
    const QSize size = output.output->sourceSize();
    glViewport(0, 0, size.width(), size.height());
}

QRegion EglGbmBackend::beginFrame(AbstractOutput *drmOutput)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    Output &output = m_outputs[drmOutput];
    if (output.scanoutSurface) {
        qCDebug(KWIN_DRM) << "Direct scanout stopped on output" << output.output->name();
    }
    output.scanoutSurface = nullptr;
    output.scanoutBuffer = nullptr;
    if (output.scanoutCandidate.surface) {
        output.oldScanoutCandidate = output.scanoutCandidate.surface;
        output.scanoutCandidate = {};
    } else if (output.oldScanoutCandidate && output.oldScanoutCandidate->dmabufFeedbackV1()) {
        output.oldScanoutCandidate->dmabufFeedbackV1()->setTranches({});
    }
    if (isPrimary()) {
        return prepareRenderingForOutput(output);
    } else {
        return renderingBackend()->beginFrame(output.output);
    }
}

bool EglGbmBackend::doesRenderFit(const Output &output, const Output::RenderData &render)
{
    if (!render.gbmSurface) {
        return false;
    }
    if (output.forceXrgb8888 && render.gbmSurface->format() != DRM_FORMAT_XRGB8888) {
        return false;
    }
    if (!output.output->isFormatSupported(render.gbmSurface->format())
        || (!render.gbmSurface->modifiers().isEmpty()
            && output.output->supportedModifiers(render.gbmSurface->format()) != render.gbmSurface->modifiers())) {
        return false;
    }
    QSize surfaceSize = output.output->bufferSize();
    if (surfaceSize != render.gbmSurface->size()) {
        return false;
    }
    bool needsTexture = output.output->needsSoftwareTransformation();
    if (needsTexture) {
        return render.shadowBuffer && render.shadowBuffer->textureSize() == output.output->sourceSize();
    } else {
        return render.shadowBuffer == nullptr;
    }
}

QRegion EglGbmBackend::prepareRenderingForOutput(Output &output)
{
    // check if the current surface still fits
    if (!doesRenderFit(output, output.current)) {
        if (doesRenderFit(output, output.old)) {
            cleanupRenderData(output.current);
            output.current = output.old;
            output.old = {};
        } else {
            resetOutput(output);
        }
    }
    makeContextCurrent(output.current);
    if (output.current.shadowBuffer) {
        output.current.shadowBuffer->bind();
    }
    setViewport(output);

    const QRect geometry = output.output->geometry();
    if (supportsBufferAge()) {
        auto current = &output.current;
        return current->damageJournal.accumulate(current->bufferAge, geometry);
    }

    return geometry;
}

QSharedPointer<DrmBuffer> EglGbmBackend::endFrameWithBuffer(AbstractOutput *drmOutput, const QRegion &dirty)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    Output &output = m_outputs[drmOutput];
    if (isPrimary()) {
        renderFramebufferToSurface(output);
        auto buffer = output.current.gbmSurface->swapBuffersForDrm();
        if (buffer) {
            updateBufferAge(output, dirty);
        }
        return buffer;
    } else {
        return importFramebuffer(output, dirty);
    }
}

void EglGbmBackend::endFrame(AbstractOutput *drmOutput, const QRegion &renderedRegion,
                             const QRegion &damagedRegion)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    Q_UNUSED(renderedRegion)

    Output &output = m_outputs[drmOutput];
    cleanupRenderData(output.old);

    const QRegion dirty = damagedRegion.intersected(output.output->geometry());
    QSharedPointer<DrmBuffer> buffer = endFrameWithBuffer(drmOutput, dirty);
    output.output->present(buffer, dirty);
}

void EglGbmBackend::updateBufferAge(Output &output, const QRegion &dirty)
{
    if (supportsBufferAge()) {
        eglQuerySurface(eglDisplay(), output.current.gbmSurface->eglSurface(), EGL_BUFFER_AGE_EXT, &output.current.bufferAge);
        output.current.damageJournal.add(dirty);
    }
}

bool EglGbmBackend::scanout(AbstractOutput *drmOutput, SurfaceItem *surfaceItem)
{
    static bool valid;
    static const bool directScanoutDisabled = qEnvironmentVariableIntValue("KWIN_DRM_NO_DIRECT_SCANOUT", &valid) == 1 && valid;
    if (directScanoutDisabled) {
        return false;
    }
    Q_ASSERT(m_outputs.contains(drmOutput));
    SurfaceItemWayland *item = qobject_cast<SurfaceItemWayland *>(surfaceItem);
    if (!item) {
        return false;
    }

    KWaylandServer::SurfaceInterface *surface = item->surface();
    if (!surface) {
        return false;
    }

    auto buffer = qobject_cast<KWaylandServer::LinuxDmaBufV1ClientBuffer *>(surface->buffer());
    if (!buffer) {
        return false;
    }
    Output &output = m_outputs[drmOutput];
    const auto planes = buffer->planes();
    if (buffer->size() != output.output->modeSize() || planes.isEmpty()) {
        return false;
    }
    if (output.oldScanoutCandidate && output.oldScanoutCandidate != surface) {
        if (output.oldScanoutCandidate->dmabufFeedbackV1()) {
            output.oldScanoutCandidate->dmabufFeedbackV1()->setTranches({});
        }
        output.oldScanoutCandidate = nullptr;
    }
    if (output.scanoutCandidate.surface != surface) {
        output.scanoutCandidate.attemptedFormats = {};
    }
    output.scanoutCandidate.surface = surface;
    const auto &sendFeedback = [&output, &buffer, &planes, this]() {
        if (!output.scanoutCandidate.attemptedFormats[buffer->format()].contains(planes.first().modifier)) {
            output.scanoutCandidate.attemptedFormats[buffer->format()] << planes.first().modifier;
        }
        if (const auto &drmOutput = qobject_cast<DrmOutput *>(output.output); drmOutput && output.scanoutCandidate.surface->dmabufFeedbackV1()) {
            QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> scanoutTranches;
            const auto &drmFormats = drmOutput->pipeline()->supportedFormats();
            const auto tranches = primaryBackend()->dmabuf()->tranches();
            for (const auto &tranche : tranches) {
                KWaylandServer::LinuxDmaBufV1Feedback::Tranche scanoutTranche;
                for (auto it = tranche.formatTable.constBegin(); it != tranche.formatTable.constEnd(); it++) {
                    const uint32_t format = it.key();
                    const auto trancheModifiers = it.value();
                    const auto drmModifiers = drmFormats[format];
                    for (const auto &mod : trancheModifiers) {
                        if (drmModifiers.contains(mod) && !output.scanoutCandidate.attemptedFormats[format].contains(mod)) {
                            scanoutTranche.formatTable[format] << mod;
                        }
                    }
                }
                if (!scanoutTranche.formatTable.isEmpty()) {
                    scanoutTranche.device = m_gpu->deviceId();
                    scanoutTranche.flags = KWaylandServer::LinuxDmaBufV1Feedback::TrancheFlag::Scanout;
                    scanoutTranches << scanoutTranche;
                }
            }
            output.scanoutCandidate.surface->dmabufFeedbackV1()->setTranches(scanoutTranches);
        }
    };
    if (!output.output->isFormatSupported(buffer->format())) {
        sendFeedback();
        return false;
    }

    gbm_bo *importedBuffer;
    if (planes.first().modifier != DRM_FORMAT_MOD_INVALID
        || planes.first().offset > 0
        || planes.count() > 1) {
        if (!m_gpu->addFB2ModifiersSupported() || !output.output->supportedModifiers(buffer->format()).contains(planes.first().modifier)) {
            sendFeedback();
            return false;
        }
        gbm_import_fd_modifier_data data = {};
        data.format = buffer->format();
        data.width = (uint32_t) buffer->size().width();
        data.height = (uint32_t) buffer->size().height();
        data.num_fds = planes.count();
        data.modifier = planes.first().modifier;
        for (int i = 0; i < planes.count(); i++) {
            data.fds[i] = planes[i].fd;
            data.offsets[i] = planes[i].offset;
            data.strides[i] = planes[i].stride;
        }
        importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_SCANOUT);
    } else {
        auto plane = planes.first();
        gbm_import_fd_data data = {};
        data.fd = plane.fd;
        data.width = (uint32_t) buffer->size().width();
        data.height = (uint32_t) buffer->size().height();
        data.stride = plane.stride;
        data.format = buffer->format();
        importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD, &data, GBM_BO_USE_SCANOUT);
    }
    if (!importedBuffer) {
        sendFeedback();
        if (errno != EINVAL) {
            qCWarning(KWIN_DRM) << "Importing buffer for direct scanout failed:" << strerror(errno);
        }
        return false;
    }
    // damage tracking for screen casting
    QRegion damage;
    if (output.scanoutSurface == surface && buffer->size() == output.output->modeSize()) {
        QRegion trackedDamage = surfaceItem->damage();
        surfaceItem->resetDamage();
        for (const auto &rect : trackedDamage) {
            auto damageRect = QRect(rect);
            damageRect.translate(output.output->geometry().topLeft());
            damage |= damageRect;
        }
    } else {
        damage = output.output->geometry();
    }
    auto bo = QSharedPointer<DrmGbmBuffer>::create(m_gpu, importedBuffer, buffer);
    if (!bo->bufferId()) {
        // buffer can't actually be scanned out. Mesa is supposed to prevent this from happening
        // in gbm_bo_import but apparently that doesn't always work
        sendFeedback();
        return false;
    }
    // ensure that a context is current like with normal presentation
    makeCurrent();
    if (output.output->present(bo, damage)) {
        if (output.scanoutSurface != surface) {
            auto path = surface->client()->executablePath();
            qCDebug(KWIN_DRM).nospace() << "Direct scanout starting on output " << output.output->name() << " for application \"" << path << "\"";
        }
        output.scanoutSurface = surface;
        output.scanoutBuffer = bo;
        return true;
    } else {
        // TODO clean the modeset and direct scanout code paths up
        if (!m_gpu->needsModeset()) {
            sendFeedback();
        }
        return false;
    }
}

QSharedPointer<DrmBuffer> EglGbmBackend::renderTestFrame(DrmAbstractOutput *output)
{
    beginFrame(output);
    glClear(GL_COLOR_BUFFER_BIT);
    const auto buffer = endFrameWithBuffer(output, output->geometry());
    if (!buffer && this != renderingBackend()) {
        // if CPU import fails we might need to try again
        beginFrame(output);
        glClear(GL_COLOR_BUFFER_BIT);
        return endFrameWithBuffer(output, output->geometry());
    } else {
        return buffer;
    }
}

QSharedPointer<GLTexture> EglGbmBackend::textureForOutput(AbstractOutput *output) const
{
    Q_ASSERT(m_outputs.contains(output));
    const auto createImage = [this, output](GbmBuffer *buffer) {
        EGLImageKHR image = eglCreateImageKHR(eglDisplay(), nullptr, EGL_NATIVE_PIXMAP_KHR, buffer->getBo(), nullptr);
        if (image == EGL_NO_IMAGE_KHR) {
            qCWarning(KWIN_DRM) << "Failed to record frame: Error creating EGLImageKHR - " << glGetError();
            return QSharedPointer<EGLImageTexture>(nullptr);
        }
        return QSharedPointer<EGLImageTexture>::create(eglDisplay(), image, GL_RGBA8, static_cast<DrmAbstractOutput*>(output)->modeSize());
    };
    auto &renderOutput = m_outputs[output];
    if (renderOutput.scanoutBuffer) {
        return createImage(dynamic_cast<GbmBuffer*>(renderOutput.scanoutBuffer.data()));
    }
    if (renderOutput.current.shadowBuffer) {
        const auto glTexture = QSharedPointer<KWin::GLTexture>::create(renderOutput.current.shadowBuffer->texture(), GL_RGBA8, renderOutput.output->sourceSize());
        glTexture->setYInverted(true);
        return glTexture;
    }
    GbmBuffer *gbmBuffer = renderOutput.current.gbmSurface->currentBuffer().get();
    if (!gbmBuffer) {
        qCWarning(KWIN_DRM) << "Failed to record frame: No gbm buffer!";
        return {};
    }
    return createImage(gbmBuffer);
}

bool EglGbmBackend::directScanoutAllowed(AbstractOutput *output) const
{
    return !output->usesSoftwareCursor() && !output->directScanoutInhibited();
}

bool EglGbmBackend::hasOutput(AbstractOutput *output) const
{
    return m_outputs.contains(output);
}

uint32_t EglGbmBackend::drmFormat(DrmAbstractOutput *output) const
{
    const auto &o = m_outputs[output];
    return o.output ? o.current.format.drmFormat : DRM_FORMAT_XRGB8888;
}

DrmGpu *EglGbmBackend::gpu() const
{
    return m_gpu;
}

std::optional<GbmFormat> EglGbmBackend::chooseFormat(Output &output) const
{
    if (output.forceXrgb8888) {
        return GbmFormat {
            .drmFormat = DRM_FORMAT_XRGB8888,
            .redSize = 8,
            .greenSize = 8,
            .blueSize = 8,
            .alphaSize = 0,
        };
    }
    // formats are already sorted by order of preference
    std::optional<GbmFormat> fallback;
    for (const auto &format : qAsConst(m_formats)) {
        if (output.output->isFormatSupported(format.drmFormat)) {
            int bpc = std::max(format.redSize, std::max(format.greenSize, format.blueSize));
            if (bpc <= output.output->maxBpc()) {
                return format;
            } else if (!fallback.has_value()) {
                fallback = format;
            }
        }
    }
    return fallback;
}

EglGbmBackend *EglGbmBackend::renderingBackend()
{
    return static_cast<EglGbmBackend*>(primaryBackend());
}

bool EglGbmBackend::prefer10bpc() const
{
    static bool ok = false;
    static const int preferred = qEnvironmentVariableIntValue("KWIN_DRM_PREFER_COLOR_DEPTH", &ok);
    return !ok || preferred == 30;
}

void EglGbmBackend::setForceXrgb8888(DrmAbstractOutput *output) {
    auto &o = m_outputs[output];
    o.forceXrgb8888 = true;
}

bool operator==(const GbmFormat &lhs, const GbmFormat &rhs)
{
    return lhs.drmFormat == rhs.drmFormat;
}

}

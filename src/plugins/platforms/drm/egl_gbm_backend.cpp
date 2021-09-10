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
    : AbstractEglDrmBackend(drmBackend, gpu)
{
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

        auto device = gbm_create_device(m_gpu->fd());
        if (!device) {
            setFailed("Could not create gbm device");
            return false;
        }
        m_gpu->setGbmDevice(device);

        display = eglGetPlatformDisplayEXT(platform, device, nullptr);
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
    const QSize size = output.output->sourceSize();
    QVector<uint64_t> modifiers = output.output->supportedModifiers(m_gbmFormat);

    QSharedPointer<GbmSurface> gbmSurface;
#if HAVE_GBM_BO_GET_FD_FOR_PLANE
    if (modifiers.isEmpty()) {
#else
    // modifiers have to be disabled with multi-gpu if gbm_bo_get_fd_for_plane is not available
    if (modifiers.isEmpty() || output.output->gpu() != m_gpu) {
#endif
        int flags = GBM_BO_USE_RENDERING;
        if (output.output->gpu() == m_gpu) {
            flags |= GBM_BO_USE_SCANOUT;
        } else {
            flags |= GBM_BO_USE_LINEAR;
        }
        gbmSurface = QSharedPointer<GbmSurface>::create(m_gpu, size, m_gbmFormat, flags);
    } else {
        gbmSurface = QSharedPointer<GbmSurface>::create(m_gpu, size, m_gbmFormat, modifiers);
        if (!gbmSurface->isValid()) {
            // the egl / gbm implementation may reject the modifier list from another gpu
            // as a fallback use linear, to at least make CPU copy more efficient
            modifiers = {DRM_FORMAT_MOD_LINEAR};
            gbmSurface = QSharedPointer<GbmSurface>::create(m_gpu, size, m_gbmFormat, modifiers);
        }
    }
    if (!gbmSurface->isValid()) {
        qCCritical(KWIN_DRM) << "Creating GBM surface failed:" << strerror(errno);
        return false;
    }
    cleanupRenderData(output.old);
    output.old = output.current;
    output.current = {};
    output.current.gbmSurface = gbmSurface;

    if (!output.output->needsSoftwareTransformation())  {
        output.current.shadowBuffer = nullptr;
    } else {
        makeContextCurrent(output.current);
        output.current.shadowBuffer = QSharedPointer<ShadowBuffer>::create(output.output->pixelSize());
        if (!output.current.shadowBuffer->isComplete()) {
            return false;
        }
    }
    return true;
}

bool EglGbmBackend::addOutput(DrmAbstractOutput *drmOutput)
{
    Output newOutput;
    newOutput.output = drmOutput;
    if (isPrimary()) {
        if (!resetOutput(newOutput)) {
            return false;
        }
    } else {
        if (!renderingBackend()->addOutput(drmOutput)) {
            return false;
        }
    }
    m_outputs.insert(drmOutput, newOutput);
    return true;
}

void EglGbmBackend::removeOutput(DrmAbstractOutput *drmOutput)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    if (isPrimary()) {
        // shadow buffer needs context current for destruction
        makeCurrent();
    } else {
        renderingBackend()->removeOutput(drmOutput);
    }
    m_outputs.remove(drmOutput);
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

QRegion EglGbmBackend::beginFrameForSecondaryGpu(DrmAbstractOutput *drmOutput)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    return prepareRenderingForOutput(m_outputs[drmOutput]);
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
        output.current.importSwapchain = QSharedPointer<DumbSwapchain>::create(m_gpu, size);
        if (output.current.importSwapchain->isEmpty()) {
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
    // TODO turn off output?
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

    qCDebug(KWIN_DRM) << "EGL buffer configs count:" << count;

    uint32_t fallbackFormat = 0;
    EGLConfig fallbackConfig = nullptr;

    // Loop through all configs, choosing the first one that has suitable format.
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat;
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);

        if (!m_gpu->isFormatSupported(gbmFormat)) {
            continue;
        }

        // Query number of bits for color channel
        EGLint blueSize, redSize, greenSize, alphaSize;
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &redSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &greenSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &blueSize);
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &alphaSize);

        // prefer XRGB8888 as it's most likely to be supported by secondary GPUs as well
        if (gbmFormat == GBM_FORMAT_XRGB8888) {
            m_gbmFormat = gbmFormat;
            setConfig(configs[i]);
            return true;
        } else if (!fallbackConfig && blueSize >= 8 && redSize >= 8 && greenSize >= 8) {
            fallbackFormat = gbmFormat;
            fallbackConfig = configs[i];
        }
    }

    if (fallbackConfig) {
        m_gbmFormat = fallbackFormat;
        setConfig(fallbackConfig);
        return true;
    }

    qCCritical(KWIN_DRM) << "Choosing EGL config did not return a suitable config. There were"
                         << count << "configs:";
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

PlatformSurfaceTexture *EglGbmBackend::createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return new BasicEGLSurfaceTextureInternal(this, pixmap);
}

PlatformSurfaceTexture *EglGbmBackend::createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return new BasicEGLSurfaceTextureWayland(this, pixmap);
}

void EglGbmBackend::setViewport(const Output &output) const
{
    const QSize size = output.output->pixelSize();
    glViewport(0, 0, size.width(), size.height());
}

QRegion EglGbmBackend::beginFrame(AbstractOutput *drmOutput)
{
    Q_ASSERT(m_outputs.contains(drmOutput));
    Output &output = m_outputs[drmOutput];
    if (output.surfaceInterface) {
        qCDebug(KWIN_DRM) << "Direct scanout stopped on output" << output.output->name();
    }
    output.surfaceInterface = nullptr;
    if (isPrimary()) {
        return prepareRenderingForOutput(output);
    } else {
        return renderingBackend()->beginFrameForSecondaryGpu(output.output);
    }
}

bool EglGbmBackend::doesRenderFit(DrmAbstractOutput *output, const Output::RenderData &render)
{
    if (!render.gbmSurface) {
        return false;
    }
    QSize surfaceSize = output->sourceSize();
    if (surfaceSize != render.gbmSurface->size()) {
        return false;
    }
    bool needsTexture = output->needsSoftwareTransformation();
    if (needsTexture) {
        return render.shadowBuffer && render.shadowBuffer->textureSize() == output->pixelSize();
    } else {
        return render.shadowBuffer == nullptr;
    }
}

QRegion EglGbmBackend::prepareRenderingForOutput(Output &output)
{
    // check if the current surface still fits
    if (!doesRenderFit(output.output, output.current)) {
        if (doesRenderFit(output.output, output.old)) {
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
    if (!buffer || !output.output->present(buffer, dirty)) {
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(output.output->renderLoop());
        renderLoopPrivate->notifyFrameFailed();
        return;
    }
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
    if (buffer->size() != output.output->modeSize()) {
        return false;
    }
    if (!buffer->planes().count()) {
        return false;
    }
    if (!output.output->isFormatSupported(buffer->format())) {
        return false;
    }

    gbm_bo *importedBuffer;
    if (buffer->planes()[0].modifier != DRM_FORMAT_MOD_INVALID
        || buffer->planes()[0].offset > 0
        || buffer->planes().size() > 1) {
        if (!m_gpu->addFB2ModifiersSupported() || !output.output->supportedModifiers(buffer->format()).contains(buffer->planes()[0].modifier)) {
            return false;
        }
        gbm_import_fd_modifier_data data = {};
        data.format = buffer->format();
        data.width = (uint32_t) buffer->size().width();
        data.height = (uint32_t) buffer->size().height();
        data.num_fds = buffer->planes().count();
        data.modifier = buffer->planes()[0].modifier;
        for (int i = 0; i < buffer->planes().count(); i++) {
            auto plane = buffer->planes()[i];
            data.fds[i] = plane.fd;
            data.offsets[i] = plane.offset;
            data.strides[i] = plane.stride;
        }
        importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_SCANOUT);
    } else {
        auto plane = buffer->planes()[0];
        gbm_import_fd_data data = {};
        data.fd = plane.fd;
        data.width = (uint32_t) buffer->size().width();
        data.height = (uint32_t) buffer->size().height();
        data.stride = plane.stride;
        data.format = buffer->format();
        importedBuffer = gbm_bo_import(m_gpu->gbmDevice(), GBM_BO_IMPORT_FD, &data, GBM_BO_USE_SCANOUT);
    }
    if (!importedBuffer) {
        if (errno != EINVAL) {
            qCWarning(KWIN_DRM) << "Importing buffer for direct scanout failed:" << strerror(errno);
        }
        return false;
    }
    // damage tracking for screen casting
    QRegion damage;
    if (output.surfaceInterface == surface && buffer->size() == output.output->modeSize()) {
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
    // ensure that a context is current like with normal presentation
    makeCurrent();
    if (output.output->present(bo, damage)) {
        output.current.damageJournal.clear();
        if (output.surfaceInterface != surface) {
            auto path = surface->client()->executablePath();
            qCDebug(KWIN_DRM).nospace() << "Direct scanout starting on output " << output.output->name() << " for application \"" << path << "\"";
        }
        output.surfaceInterface = surface;
        return true;
    } else {
        return false;
    }
}

QSharedPointer<DrmBuffer> EglGbmBackend::renderTestFrame(DrmAbstractOutput *output)
{
    beginFrame(output);
    glClear(GL_COLOR_BUFFER_BIT);
    return endFrameWithBuffer(output, output->geometry());
}

QSharedPointer<GLTexture> EglGbmBackend::textureForOutput(AbstractOutput *output) const
{
    Q_ASSERT(m_outputs.contains(output));
    auto &renderOutput = m_outputs[output];
    if (renderOutput.current.shadowBuffer) {
        const auto glTexture = QSharedPointer<KWin::GLTexture>::create(renderOutput.current.shadowBuffer->texture(), GL_RGBA8, output->pixelSize());
        glTexture->setYInverted(true);
        return glTexture;
    }
    GbmBuffer *gbmBuffer = renderOutput.current.gbmSurface->currentBuffer().get();
    if (!gbmBuffer) {
        qCWarning(KWIN_DRM) << "Failed to record frame: No gbm buffer!";
        return {};
    }
    EGLImageKHR image = eglCreateImageKHR(eglDisplay(), nullptr, EGL_NATIVE_PIXMAP_KHR, gbmBuffer->getBo(), nullptr);
    if (image == EGL_NO_IMAGE_KHR) {
        qCWarning(KWIN_DRM) << "Failed to record frame: Error creating EGLImageKHR - " << glGetError();
        return {};
    }

    return QSharedPointer<EGLImageTexture>::create(eglDisplay(), image, GL_RGBA8, static_cast<DrmAbstractOutput*>(output)->modeSize());
}

bool EglGbmBackend::directScanoutAllowed(AbstractOutput *output) const
{
    return !m_backend->usesSoftwareCursor() && output->directScanoutInhibited();
}

bool EglGbmBackend::hasOutput(AbstractOutput *output) const
{
    return m_outputs.contains(output);
}

}

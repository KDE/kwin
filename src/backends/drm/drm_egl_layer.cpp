/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_egl_layer.h"
#include "drm_abstract_output.h"
#include "drm_backend.h"
#include "drm_buffer_gbm.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "egl_dmabuf.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/surface_interface.h"

#include <QRegion>
#include <drm_fourcc.h>
#include <errno.h>
#include <gbm.h>
#include <unistd.h>

namespace KWin
{

static TextureTransforms drmToTextureRotation(DrmPipeline *pipeline)
{
    auto angle = DrmPlane::transformationToDegrees(pipeline->renderOrientation()) - DrmPlane::transformationToDegrees(pipeline->bufferOrientation());
    if (angle < 0) {
        angle += 360;
    }
    switch (angle % 360) {
    case 0:
        return TextureTransforms();
    case 90:
        return TextureTransform::Rotate90;
    case 180:
        return TextureTransform::Rotate180;
    case 270:
        return TextureTransform::Rotate270;
    default:
        Q_UNREACHABLE();
    }
}

EglGbmLayer::EglGbmLayer(EglGbmBackend *eglBackend, DrmPipeline *pipeline)
    : DrmPipelineLayer(pipeline)
    , m_surface(pipeline->gpu(), eglBackend)
    , m_dmabufFeedback(pipeline->gpu(), eglBackend)
{
}

std::optional<OutputLayerBeginFrameInfo> EglGbmLayer::beginFrame()
{
    m_scanoutBuffer.reset();
    m_dmabufFeedback.renderingSurface();

    return m_surface.startRendering(m_pipeline->bufferSize(), drmToTextureRotation(m_pipeline) | TextureTransform::MirrorY, m_pipeline->formats());
}

bool EglGbmLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    const bool ret = m_surface.endRendering(damagedRegion);
    if (ret) {
        m_currentDamage = damagedRegion;
    }
    return ret;
}

QRegion EglGbmLayer::currentDamage() const
{
    return m_currentDamage;
}

bool EglGbmLayer::checkTestBuffer()
{
    return m_surface.renderTestBuffer(m_pipeline->bufferSize(), m_pipeline->formats()) != nullptr;
}

std::shared_ptr<GLTexture> EglGbmLayer::texture() const
{
    if (m_scanoutBuffer) {
        const auto ret = m_surface.eglBackend()->importBufferObjectAsTexture(static_cast<GbmBuffer *>(m_scanoutBuffer->buffer())->bo());
        ret->setContentTransform(drmToTextureRotation(m_pipeline));
        return ret;
    } else {
        return m_surface.texture();
    }
}

static DrmPlane::Transformations invertAndConvertTransform(Output::Transform transform)
{
    DrmPlane::Transformations ret;
    switch (transform) {
    case Output::Transform::Normal:
    case Output::Transform::Flipped:
        ret = DrmPlane::Transformation::Rotate0;
        break;
    case Output::Transform::Rotated90:
    case Output::Transform::Flipped90:
        ret = DrmPlane::Transformation::Rotate270;
        break;
    case Output::Transform::Rotated180:
    case Output::Transform::Flipped180:
        ret = DrmPlane::Transformation::Rotate180;
        break;
    case Output::Transform::Rotated270:
    case Output::Transform::Flipped270:
        ret = DrmPlane::Transformation::Rotate90;
        break;
    }
    if (transform == Output::Transform::Flipped || transform == Output::Transform::Flipped180) {
        ret |= DrmPlane::Transformation::ReflectX;
    } else if (transform == Output::Transform::Flipped90 || transform == Output::Transform::Flipped270) {
        ret |= DrmPlane::Transformation::ReflectY;
    }
    return ret;
}

bool EglGbmLayer::scanout(SurfaceItem *surfaceItem)
{
    static bool valid;
    static const bool directScanoutDisabled = qEnvironmentVariableIntValue("KWIN_DRM_NO_DIRECT_SCANOUT", &valid) == 1 && valid;
    if (directScanoutDisabled) {
        return false;
    }

    SurfaceItemWayland *item = qobject_cast<SurfaceItemWayland *>(surfaceItem);
    if (!item || !item->surface()) {
        return false;
    }
    const auto surface = item->surface();
    if (invertAndConvertTransform(surface->bufferTransform()) != m_pipeline->bufferOrientation()) {
        return false;
    }
    const auto buffer = qobject_cast<KWaylandServer::LinuxDmaBufV1ClientBuffer *>(surface->buffer());
    if (!buffer) {
        return false;
    }

    const auto formats = m_pipeline->formats();
    if (!formats.contains(buffer->format())) {
        m_dmabufFeedback.scanoutFailed(surface, formats);
        return false;
    }
    if (buffer->attributes().modifier == DRM_FORMAT_MOD_INVALID && m_pipeline->gpu()->platform()->gpuCount() > 1) {
        // importing a buffer from another GPU without an explicit modifier can mess up the buffer format
        return false;
    }
    if (!formats[buffer->format()].contains(buffer->attributes().modifier)) {
        return false;
    }
    const auto gbmBuffer = GbmBuffer::importBuffer(m_pipeline->gpu(), buffer);
    if (!gbmBuffer) {
        m_dmabufFeedback.scanoutFailed(surface, formats);
        return false;
    }
    m_scanoutBuffer = DrmFramebuffer::createFramebuffer(gbmBuffer);
    if (m_scanoutBuffer && m_pipeline->testScanout()) {
        m_dmabufFeedback.scanoutSuccessful(surface);
        m_currentDamage = surfaceItem->damage();
        surfaceItem->resetDamage();
        // ensure the pixmap is updated when direct scanout ends
        surfaceItem->destroyPixmap();
        return true;
    } else {
        m_dmabufFeedback.scanoutFailed(surface, formats);
        m_scanoutBuffer.reset();
        return false;
    }
}

std::shared_ptr<DrmFramebuffer> EglGbmLayer::currentBuffer() const
{
    return m_scanoutBuffer ? m_scanoutBuffer : m_surface.currentBuffer();
}

bool EglGbmLayer::hasDirectScanoutBuffer() const
{
    return m_scanoutBuffer != nullptr;
}

void EglGbmLayer::releaseBuffers()
{
    m_scanoutBuffer.reset();
    m_surface.destroyResources();
}
}


#include "egl_gbm_overlay_layer.h"

#include "drm_buffer_gbm.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "egl_gbm_backend.h"
#include "surfaceitem_wayland.h"

#include <KWaylandServer/linuxdmabufv1clientbuffer.h>
#include <KWaylandServer/surface_interface.h>

namespace KWin
{

EglGbmOverlayLayer::EglGbmOverlayLayer(EglGbmBackend *eglBackend, DrmPlane *overlayPlane, DrmPipeline *pipeline)
    : m_surface(overlayPlane->gpu(), eglBackend)
    , m_dmabufFeedback(overlayPlane->gpu(), eglBackend)
    , m_plane(overlayPlane)
    , m_pipeline(pipeline)
{
}

EglGbmOverlayLayer::~EglGbmOverlayLayer()
{
    destroyResources();
}

void EglGbmOverlayLayer::destroyResources()
{
    m_surface.destroyResources();
}

std::optional<QRegion> EglGbmOverlayLayer::beginFrame(const QRect &geometry)
{
    m_geometry = geometry;
    m_dmabufFeedback.renderingSurface();

    const QMatrix4x4 matrix = AbstractWaylandOutput::logicalToNativeMatrix(m_pipeline->output()->geometry(), m_pipeline->output()->scale(), m_pipeline->output()->transform());
    m_pixelGeometry = matrix.mapRect(geometry);

    return m_surface.startRendering(m_pixelGeometry.size(), geometry, m_pipeline->pending.bufferTransformation, m_pipeline->pending.sourceTransformation, m_pipeline->supportedFormats());
}

void EglGbmOverlayLayer::aboutToStartPainting(const QRegion &damagedRegion)
{
    m_surface.aboutToStartPainting(m_pipeline->output(), damagedRegion);
}

void EglGbmOverlayLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    const auto ret = m_surface.endRendering(m_pipeline->pending.bufferTransformation, damagedRegion);
    if (ret.has_value()) {
        std::tie(m_currentBuffer, m_currentDamage) = ret.value();
    }
}

QRegion EglGbmOverlayLayer::currentDamage() const
{
    return m_currentDamage;
}

QSharedPointer<GLTexture> EglGbmOverlayLayer::texture() const
{
    return m_surface.texture();
}

bool EglGbmOverlayLayer::scanout(SurfaceItem *surfaceItem)
{
    // TODO support scanout with overlays
    Q_UNUSED(surfaceItem)
    return false;
}

QSharedPointer<DrmBuffer> EglGbmOverlayLayer::currentBuffer() const
{
    return m_currentBuffer;
}

QRect EglGbmOverlayLayer::pixelGeometry() const
{
    return m_pixelGeometry;
}

QRect EglGbmOverlayLayer::geometry() const
{
    return m_geometry;
}

void EglGbmOverlayLayer::pageFlipped()
{
    m_currentBuffer.reset();
}
}

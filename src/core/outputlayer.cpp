/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputlayer.h"
#include "scene/surfaceitem.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/surface.h"

namespace KWin
{

OutputLayer::OutputLayer(Output *output)
    : m_output(output)
{
}

QPointF OutputLayer::hotspot() const
{
    return m_hotspot;
}

void OutputLayer::setHotspot(const QPointF &hotspot)
{
    m_hotspot = hotspot;
}

QList<QSize> OutputLayer::recommendedSizes() const
{
    return {};
}

QRegion OutputLayer::repaints() const
{
    return m_repaints;
}

void OutputLayer::scheduleRepaint(Item *item)
{
    m_repaintScheduled = true;
    m_output->renderLoop()->scheduleRepaint(item, this);
    Q_EMIT repaintScheduled();
}

void OutputLayer::addRepaint(const QRegion &region)
{
    if (region.isEmpty()) {
        return;
    }
    m_repaints += region;
    m_output->renderLoop()->scheduleRepaint(nullptr, this);
    Q_EMIT repaintScheduled();
}

void OutputLayer::resetRepaints()
{
    m_repaintScheduled = false;
    m_repaints = QRegion();
}

bool OutputLayer::needsRepaint() const
{
    return m_repaintScheduled || !m_repaints.isEmpty();
}

bool OutputLayer::doImportScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    return false;
}

bool OutputLayer::importScanoutBuffer(SurfaceItem *surfaceItem, const std::shared_ptr<OutputFrame> &frame)
{
    SurfaceItemWayland *wayland = qobject_cast<SurfaceItemWayland *>(surfaceItem);
    if (!wayland || !wayland->surface()) {
        return false;
    }
    const auto buffer = wayland->surface()->buffer();
    if (!buffer) {
        return false;
    }
    const auto attrs = buffer->dmabufAttributes();
    if (!attrs) {
        return false;
    }
    const bool tearing = frame->presentationMode() == PresentationMode::Async || frame->presentationMode() == PresentationMode::AdaptiveAsync;
    const auto formats = tearing ? supportedAsyncDrmFormats() : supportedDrmFormats();
    if (auto it = formats.find(attrs->format); it != formats.end() && !it->contains(attrs->modifier)) {
        if (m_scanoutCandidate && m_scanoutCandidate != surfaceItem) {
            m_scanoutCandidate->setScanoutHint(nullptr, {});
        }
        m_scanoutCandidate = surfaceItem;
        surfaceItem->setScanoutHint(scanoutDevice(), formats);
        return false;
    }
    m_sourceRect = surfaceItem->bufferSourceBox();
    m_bufferTransform = surfaceItem->bufferTransform();
    const auto desiredTransform = m_output ? m_output->transform() : OutputTransform::Kind::Normal;
    m_offloadTransform = m_bufferTransform.combine(desiredTransform.inverted());
    setColor(surfaceItem->colorDescription(), surfaceItem->renderingIntent(), ColorPipeline::create(surfaceItem->colorDescription(), m_output->layerBlendingColor(), surfaceItem->renderingIntent()));
    const bool ret = doImportScanoutBuffer(buffer, frame);
    if (ret) {
        surfaceItem->resetDamage();
        // ensure the pixmap is updated when direct scanout ends
        surfaceItem->destroyPixmap();
    }
    return ret;
}

std::optional<OutputLayerBeginFrameInfo> OutputLayer::beginFrame()
{
    m_sourceRect = QRectF(QPointF(0, 0), m_targetRect.size());
    m_bufferTransform = m_output ? m_output->transform() : OutputTransform::Kind::Normal;
    m_offloadTransform = OutputTransform::Kind::Normal;
    setColor(m_output->layerBlendingColor(), RenderingIntent::AbsoluteColorimetric, ColorPipeline{});
    return doBeginFrame();
}

bool OutputLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return doEndFrame(renderedRegion, damagedRegion, frame);
}

void OutputLayer::notifyNoScanoutCandidate()
{
    if (m_scanoutCandidate) {
        m_scanoutCandidate->setScanoutHint(nullptr, {});
        m_scanoutCandidate = nullptr;
    }
}

void OutputLayer::setEnabled(bool enable)
{
    m_enabled = enable;
}

bool OutputLayer::isEnabled() const
{
    return m_enabled;
}

QRectF OutputLayer::sourceRect() const
{
    return m_sourceRect;
}

void OutputLayer::setSourceRect(const QRectF &rect)
{
    m_sourceRect = rect;
}

OutputTransform OutputLayer::offloadTransform() const
{
    return m_offloadTransform;
}

OutputTransform OutputLayer::bufferTransform() const
{
    return m_bufferTransform;
}

QRect OutputLayer::targetRect() const
{
    return m_targetRect;
}

void OutputLayer::setTargetRect(const QRect &rect)
{
    m_targetRect = rect;
}

QHash<uint32_t, QList<uint64_t>> OutputLayer::supportedAsyncDrmFormats() const
{
    return supportedDrmFormats();
}

void OutputLayer::setOffloadTransform(const OutputTransform &transform)
{
    m_offloadTransform = transform;
}

void OutputLayer::setBufferTransform(const OutputTransform &transform)
{
    m_bufferTransform = transform;
}

const ColorPipeline &OutputLayer::colorPipeline() const
{
    return m_colorPipeline;
}

const ColorDescription &OutputLayer::colorDescription() const
{
    return m_color;
}

RenderingIntent OutputLayer::renderIntent() const
{
    return m_renderingIntent;
}

void OutputLayer::setColor(const ColorDescription &color, RenderingIntent intent, const ColorPipeline &pipeline)
{
    m_color = color;
    m_renderingIntent = intent;
    m_colorPipeline = pipeline;
}

bool OutputLayer::preparePresentationTest()
{
    return true;
}

} // namespace KWin

#include "moc_outputlayer.cpp"

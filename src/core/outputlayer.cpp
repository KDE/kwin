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

qreal OutputLayer::scale() const
{
    return m_scale;
}

void OutputLayer::setScale(qreal scale)
{
    m_scale = scale;
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

void OutputLayer::addRepaint(const QRegion &region)
{
    m_repaints += region;
    if (m_output) {
        m_output->renderLoop()->scheduleRepaint(nullptr, nullptr, this);
    }
}

void OutputLayer::resetRepaints()
{
    m_repaints = QRegion();
}

bool OutputLayer::needsRepaint() const
{
    return !m_repaints.isEmpty();
}

bool OutputLayer::doImportScanoutBuffer(GraphicsBuffer *buffer, const ColorDescription &color, RenderingIntent intent, const std::shared_ptr<OutputFrame> &frame)
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
    const auto formats = supportedDrmFormats();
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
    const bool ret = doImportScanoutBuffer(buffer, surfaceItem->colorDescription(), surfaceItem->renderingIntent(), frame);
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

} // namespace KWin

#include "moc_outputlayer.cpp"

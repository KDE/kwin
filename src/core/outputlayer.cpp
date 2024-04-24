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

OutputLayer::OutputLayer(QObject *parent)
    : QObject(parent)
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

QSizeF OutputLayer::size() const
{
    return m_size;
}

void OutputLayer::setSize(const QSizeF &size)
{
    m_size = size;
}

std::optional<QSize> OutputLayer::fixedSize() const
{
    return std::nullopt;
}

QRegion OutputLayer::repaints() const
{
    return m_repaints;
}

void OutputLayer::addRepaint(const QRegion &region)
{
    m_repaints += region;
}

void OutputLayer::resetRepaints()
{
    m_repaints = QRegion();
}

bool OutputLayer::needsRepaint() const
{
    return !m_repaints.isEmpty();
}

bool OutputLayer::doAttemptScanout(GraphicsBuffer *buffer, const QRectF &sourceRect, const QSizeF &targetSize, OutputTransform transform, const ColorDescription &color)
{
    return false;
}

bool OutputLayer::attemptScanout(SurfaceItem *surfaceItem)
{
    SurfaceItemWayland *wayland = qobject_cast<SurfaceItemWayland *>(surfaceItem);
    if (!wayland || !wayland->surface()) {
        return false;
    }
    const auto buffer = wayland->surface()->buffer();
    const auto attrs = buffer->dmabufAttributes();
    if (!attrs) {
        return false;
    }
    const auto formats = supportedDrmFormats();
    if (!formats.contains(attrs->format) || !formats[attrs->format].contains(attrs->modifier)) {
        if (m_scanoutCandidate && m_scanoutCandidate != surfaceItem) {
            m_scanoutCandidate->setScanoutHint(nullptr, {});
        }
        m_scanoutCandidate = surfaceItem;
        surfaceItem->setScanoutHint(scanoutDevice(), supportedDrmFormats());
        return false;
    }
    const bool ret = doAttemptScanout(buffer, surfaceItem->bufferSourceBox(), surfaceItem->destinationSize(), surfaceItem->bufferTransform(), surfaceItem->colorDescription());
    if (ret) {
        surfaceItem->resetDamage();
        // ensure the pixmap is updated when direct scanout ends
        surfaceItem->destroyPixmap();
    }
    return ret;
}

void OutputLayer::notifyNoScanoutCandidate()
{
    if (m_scanoutCandidate) {
        m_scanoutCandidate->setScanoutHint(nullptr, {});
        m_scanoutCandidate = nullptr;
    }
}

void OutputLayer::setPosition(const QPointF &position)
{
    m_position = position;
}

QPointF OutputLayer::position() const
{
    return m_position;
}

void OutputLayer::setEnabled(bool enable)
{
    m_enabled = enable;
}

bool OutputLayer::isEnabled() const
{
    return m_enabled;
}

} // namespace KWin

#include "moc_outputlayer.cpp"

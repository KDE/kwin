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

OutputLayer::OutputLayer(LogicalOutput *output)
    : m_output(output)
    , m_renderLoop(output->renderLoop())
{
}

void OutputLayer::setRenderLoop(RenderLoop *loop)
{
    m_renderLoop = loop;
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
    if (m_renderLoop) {
        m_renderLoop->scheduleRepaint(item, this);
    }
    Q_EMIT repaintScheduled();
}

void OutputLayer::addRepaint(const QRegion &region)
{
    if (region.isEmpty()) {
        return;
    }
    m_repaints += region;
    if (m_renderLoop) {
        m_renderLoop->scheduleRepaint(nullptr, this);
    }
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

bool OutputLayer::importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    return false;
}

std::optional<OutputLayerBeginFrameInfo> OutputLayer::beginFrame()
{
    return doBeginFrame();
}

bool OutputLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    return doEndFrame(renderedRegion, damagedRegion, frame);
}

void OutputLayer::setScanoutCandidate(SurfaceItem *item)
{
    if (m_scanoutCandidate && item != m_scanoutCandidate) {
        m_scanoutCandidate->setScanoutHint(nullptr, {});
    }
    m_scanoutCandidate = item;
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

void OutputLayer::setRequiredAlphaBits(uint32_t bits)
{
    m_requiredAlphaBits = bits;
}

QList<FormatInfo> OutputLayer::filterAndSortFormats(const QHash<uint32_t, QList<uint64_t>> &formats, uint32_t requiredAlphaBits, LogicalOutput::ColorPowerTradeoff tradeoff)
{
    QList<FormatInfo> ret;
    for (auto it = formats.begin(); it != formats.end(); it++) {
        const auto info = FormatInfo::get(it.key());
        if (!info) {
            continue;
        }
        if (info->alphaBits < requiredAlphaBits) {
            continue;
        }
        if (info->bitsPerColor < 8) {
            continue;
        }
        ret.push_back(*info);
    }
    std::ranges::sort(ret, [tradeoff](const FormatInfo &before, const FormatInfo &after) {
        if (tradeoff == LogicalOutput::ColorPowerTradeoff::PreferAccuracy && before.bitsPerColor != after.bitsPerColor) {
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

} // namespace KWin

#include "moc_outputlayer.cpp"

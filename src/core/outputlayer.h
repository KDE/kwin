
/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/colorpipeline.h"
#include "core/rendertarget.h"
#include "kwin_export.h"

#include <QObject>
#include <QPointer>
#include <QRegion>
#include <chrono>
#include <optional>

namespace KWin
{

class SurfaceItem;
class DrmDevice;
class GraphicsBuffer;
class OutputFrame;
class GLTexture;

struct OutputLayerBeginFrameInfo
{
    RenderTarget renderTarget;
    QRegion repaint;
};

enum class OutputLayerType {
    /**
     * Required for driving an output
     */
    Primary,
    /**
     * Can only be used for cursor, or cursor-attached items,
     * as the layer may be moved asynchronously by a different process
     * (like the host compositor in a nested session)
     */
    CursorOnly,
    /**
     * Should be preferred to normal overlays when possible, as they're
     * often more efficient (but often come with size restrictions)
     */
    EfficientOverlay,
    /**
     * Generic over- or underlay
     */
    GenericLayer,
};

class KWIN_EXPORT OutputLayer : public QObject
{
    Q_OBJECT
public:
    explicit OutputLayer(Output *output, OutputLayerType type);
    explicit OutputLayer(Output *output, OutputLayerType type, int zpos);

    OutputLayerType type() const;

    void setOutput(Output *output);

    QPointF hotspot() const;
    void setHotspot(const QPointF &hotspot);

    /**
     * For some layers it can be beneficial to use specific sizes only.
     * This returns those specific sizes, if present
     */
    virtual QList<QSize> recommendedSizes() const;

    QRegion repaints() const;
    void resetRepaints();
    void scheduleRepaint(Item *item);
    /**
     * adds a repaint in layer-local logical coordinates
     */
    void addRepaint(const QRegion &region);
    bool needsRepaint() const;

    /**
     * Enables or disables this layer. Note that disabling the primary layer will cause problems
     */
    void setEnabled(bool enable);
    bool isEnabled() const;

    /**
     * If the output backend needs to test presentations,
     * the layer should override this function to allocate buffers for the test
     */
    virtual bool preparePresentationTest();

    std::optional<OutputLayerBeginFrameInfo> beginFrame();
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame);

    /**
     * Tries to import the newest buffer of the surface for direct scanout and does some early checks
     * for whether or not direct scanout *could* be successful
     * A presentation request on the output must however be used afterwards to find out if it's actually successful!
     */
    bool importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame);

    void setScanoutCandidate(SurfaceItem *item);

    virtual DrmDevice *scanoutDevice() const = 0;
    virtual QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const = 0;
    virtual QHash<uint32_t, QList<uint64_t>> supportedAsyncDrmFormats() const;

    /**
     * Returns the source rect this output layer should sample from, in buffer local coordinates
     */
    QRectF sourceRect() const;
    void setSourceRect(const QRectF &rect);
    /**
     * Returns the target rect this output layer should be shown at, in device coordinates
     */
    QRect targetRect() const;
    void setTargetRect(const QRect &rect);
    /**
     * Returns the transform this layer will apply to content passed to it
     */
    OutputTransform offloadTransform() const;
    void setOffloadTransform(const OutputTransform &transform);
    /**
     * Returns the transform a buffer passed into this layer already has
     */
    OutputTransform bufferTransform() const;
    void setBufferTransform(const OutputTransform &transform);

    const ColorPipeline &colorPipeline() const;
    const ColorDescription &colorDescription() const;
    RenderingIntent renderIntent() const;
    void setColor(const ColorDescription &color, RenderingIntent intent, const ColorPipeline &pipeline);

    virtual std::pair<std::shared_ptr<GLTexture>, ColorDescription> texture() const;

    int zpos() const;

Q_SIGNALS:
    void repaintScheduled();

protected:
    virtual bool doImportScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame);
    virtual std::optional<OutputLayerBeginFrameInfo> doBeginFrame() = 0;
    virtual bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) = 0;

    const OutputLayerType m_type;
    QRegion m_repaints;
    QPointF m_hotspot;
    QRectF m_sourceRect;
    QRect m_targetRect;
    qreal m_scale = 1.0;
    bool m_enabled = false;
    OutputTransform m_offloadTransform = OutputTransform::Kind::Normal;
    OutputTransform m_bufferTransform = OutputTransform::Kind::Normal;
    ColorPipeline m_colorPipeline;
    ColorDescription m_color = ColorDescription::sRGB;
    RenderingIntent m_renderingIntent = RenderingIntent::Perceptual;
    QPointer<SurfaceItem> m_scanoutCandidate;
    QPointer<Output> m_output;
    bool m_repaintScheduled = false;
    int m_zpos = 0;
};

} // namespace KWin

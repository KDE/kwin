/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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

struct OutputLayerBeginFrameInfo
{
    RenderTarget renderTarget;
    QRegion repaint;
};

class KWIN_EXPORT OutputLayer : public QObject
{
    Q_OBJECT
public:
    explicit OutputLayer(Output *output);

    qreal scale() const;
    void setScale(qreal scale);

    QPointF hotspot() const;
    void setHotspot(const QPointF &hotspot);

    /**
     * For most drm drivers, the buffer used for the cursor has to have a fixed size.
     * If such a fixed size is required by the backend, this function should return it
     */
    virtual std::optional<QSize> fixedSize() const;

    QRegion repaints() const;
    void resetRepaints();
    void addRepaint(const QRegion &region);
    bool needsRepaint() const;

    /**
     * Enables or disables this layer. Note that disabling the primary layer will cause problems
     */
    void setEnabled(bool enable);
    bool isEnabled() const;

    std::optional<OutputLayerBeginFrameInfo> beginFrame();
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame);

    /**
     * Tries to import the newest buffer of the surface for direct scanout
     * and does backend-specific checks if scanout can succeed
     */
    bool importScanoutBuffer(SurfaceItem *item);

    /**
     * Notifies the surface that direct scanout has been successful,
     * so that it can handle damage tracking correctly
     * TODO get rid of this
     */
    virtual void notifyScanoutSuccessful();

    /**
     * Notify that there's no scanout candidate this frame
     */
    void notifyNoScanoutCandidate();

    virtual DrmDevice *scanoutDevice() const = 0;
    virtual QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const = 0;

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
    /**
     * Returns the transform a buffer passed into this layer already has
     */
    OutputTransform bufferTransform() const;

protected:
    virtual bool doImportScanoutBuffer(GraphicsBuffer *buffer, const ColorDescription &color);
    virtual std::optional<OutputLayerBeginFrameInfo> doBeginFrame() = 0;
    virtual bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) = 0;

    QRegion m_repaints;
    QPointF m_hotspot;
    QRectF m_sourceRect;
    QRect m_targetRect;
    qreal m_scale = 1.0;
    bool m_enabled = false;
    OutputTransform m_offloadTransform = OutputTransform::Kind::Normal;
    OutputTransform m_bufferTransform = OutputTransform::Kind::Normal;
    QPointer<SurfaceItem> m_scanoutCandidate;
    Output *const m_output;
};

} // namespace KWin

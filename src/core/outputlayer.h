/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rendertarget.h"
#include "kwin_export.h"

#include <QObject>
#include <QRegion>
#include <chrono>
#include <optional>

namespace KWin
{

class SurfaceItem;

struct OutputLayerBeginFrameInfo
{
    RenderTarget renderTarget;
    QRegion repaint;
};

class KWIN_EXPORT OutputLayer : public QObject
{
    Q_OBJECT
public:
    explicit OutputLayer(QObject *parent = nullptr);

    qreal scale() const;
    void setScale(qreal scale);

    QPointF hotspot() const;
    void setHotspot(const QPointF &hotspot);

    QSizeF size() const;
    void setSize(const QSizeF &size);
    /**
     * For most drm drivers, the buffer used for the cursor has to have a fixed size.
     * If such a fixed size is required by the backend, this function should return it
     */
    virtual std::optional<QSize> fixedSize() const;

    QRegion repaints() const;
    void resetRepaints();
    void addRepaint(const QRegion &region);

    /**
     * @arg position in device coordinates
     */
    void setPosition(const QPointF &position);
    QPointF position() const;

    /**
     * Enables or disables this layer. Note that disabling the primary layer will cause problems
     */
    void setEnabled(bool enable);
    bool isEnabled() const;

    virtual std::optional<OutputLayerBeginFrameInfo> beginFrame() = 0;
    virtual bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) = 0;

    /**
     * Format in which the output data is internally stored in a drm fourcc format
     */
    virtual quint32 format() const = 0;

    /**
     * Tries to import the newest buffer of the surface for direct scanout
     * Returns @c true if scanout succeeds, @c false if rendering is necessary
     */
    virtual bool scanout(SurfaceItem *surfaceItem);

    /**
     * queries the render time of the last frame. If rendering isn't complete yet, this may block until it is
     */
    virtual std::chrono::nanoseconds queryRenderTime() const = 0;

private:
    QRegion m_repaints;
    QPointF m_hotspot;
    QPointF m_position;
    QSizeF m_size;
    qreal m_scale = 1.0;
    bool m_enabled = false;
};

} // namespace KWin

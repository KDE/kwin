/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "libkwineffects/regionf.h"
#include "libkwineffects/rendertarget.h"

#include <QObject>
#include <optional>

namespace KWin
{

class SurfaceItem;

struct OutputLayerBeginFrameInfo
{
    RenderTarget renderTarget;
    RegionF repaint;
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

    RegionF repaints() const;
    void resetRepaints();
    void addRepaint(const RegionF &region);

    virtual std::optional<OutputLayerBeginFrameInfo> beginFrame() = 0;
    virtual bool endFrame(const RegionF &renderedRegion, const RegionF &damagedRegion) = 0;

    /**
     * Format in which the output data is internally stored in a drm fourcc format
     */
    virtual quint32 format() const = 0;

    /**
     * Tries to import the newest buffer of the surface for direct scanout
     * Returns @c true if scanout succeeds, @c false if rendering is necessary
     */
    virtual bool scanout(SurfaceItem *surfaceItem);

private:
    RegionF m_repaints;
    QPointF m_hotspot;
    QSizeF m_size;
    qreal m_scale = 1.0;
};

} // namespace KWin

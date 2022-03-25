/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QRegion>
#include <optional>

class QImage;

namespace KWin
{

class SurfaceItem;

class KWIN_EXPORT OutputLayer : public QObject
{
    Q_OBJECT

public:
    explicit OutputLayer(QObject *parent = nullptr);

    QRegion repaints() const;
    void resetRepaints();
    void addRepaint(const QRegion &region);

    /**
     * Notifies about starting to paint.
     *
     * @p damage contains the reported damage as suggested by windows and effects on prepaint calls.
     */
    virtual void aboutToStartPainting(const QRegion &damage);

    virtual std::optional<QRegion> beginFrame(const QRect &geometry) = 0;
    virtual void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) = 0;

    /**
     * Tries to directly scan out a surface to the screen
     * Returns @c true if scanout succeeds, @c false if rendering is necessary
     */
    virtual bool scanout(SurfaceItem *surfaceItem);

    /**
     * for QPainter based rendering, returns the image to render to
     * default implementation returns nullptr
     */
    virtual QImage *image();

    virtual QRect geometry() const = 0;

private:
    QRegion m_repaints;
};

} // namespace KWin

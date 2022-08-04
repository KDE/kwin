/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "rendertarget.h"

#include <QObject>
#include <QRegion>
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

    QRegion repaints() const;
    void resetRepaints();
    void addRepaint(const QRegion &region);

    /**
     * Notifies about starting to paint.
     *
     * @p damage contains the reported damage as suggested by windows and effects on prepaint calls.
     */
    virtual void aboutToStartPainting(const QRegion &damage);

    virtual std::optional<OutputLayerBeginFrameInfo> beginFrame() = 0;
    virtual bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) = 0;

    /**
     * Tries to import the newest buffer of the surface for direct scanout
     * Returns @c true if scanout succeeds, @c false if rendering is necessary
     */
    virtual bool scanout(SurfaceItem *surfaceItem);

private:
    QRegion m_repaints;
};

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

#include <QObject>

namespace KWin
{

class AbstractOutput;
class OverlayWindow;
class SurfaceItem;

/**
 * The RenderBackend class is the base class for all rendering backends.
 */
class KWIN_EXPORT RenderBackend : public QObject
{
    Q_OBJECT

public:
    explicit RenderBackend(QObject *parent = nullptr);

    virtual CompositingType compositingType() const = 0;
    virtual OverlayWindow *overlayWindow() const;

    virtual bool checkGraphicsReset();

    /**
     * Notifies about starting to paint.
     *
     * @p damage contains the reported damage as suggested by windows and effects on prepaint calls.
     */
    virtual void aboutToStartPainting(AbstractOutput *output, const QRegion &damage);

    virtual QRegion beginFrame(AbstractOutput *output) = 0;
    virtual void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) = 0;

    /**
     * Tries to directly scan out a surface to the screen
     * Returns @c true if scanout succeeds, @c false if rendering is necessary
     */
    virtual bool scanout(AbstractOutput *output, SurfaceItem *surfaceItem);
};

} // namespace KWin

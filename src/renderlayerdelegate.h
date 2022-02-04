/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QRegion>

namespace KWin
{

class RenderLayer;
class SurfaceItem;

/**
 * The RenderLayerDelegate class represents a render layer's contents.
 */
class KWIN_EXPORT RenderLayerDelegate : public QObject
{
    Q_OBJECT

public:
    explicit RenderLayerDelegate(QObject *parent = nullptr);

    RenderLayer *layer() const;
    void setLayer(RenderLayer *layer);

    /**
     * This function is called by the compositor before starting compositing. Reimplement
     * this function to do frame initialization.
     */
    virtual void prePaint();

    /**
     * This function is called by the compositor after finishing compositing. Reimplement
     * this function to do post frame cleanup.
     */
    virtual void postPaint();

    /**
     * Returns the direct scanout candidate hint. It can be used to avoid compositing the
     * render layer.
     */
    virtual SurfaceItem *scanoutCandidate() const;

    /**
     * This function is called when the compositor wants the render layer delegate
     * to repaint its contents.
     */
    virtual void paint(const QRegion &damage, const QRegion &repaint, QRegion &update, QRegion &valid) = 0;

private:
    RenderLayer *m_layer = nullptr;
};

} // namespace KWin

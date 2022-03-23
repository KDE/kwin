/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QRect>
#include <QScopedPointer>

#include "outputlayer.h"

namespace KWin
{

class AbstractOutput;

class KWIN_EXPORT RenderOutput : public QObject
{
    Q_OBJECT
public:
    RenderOutput(AbstractOutput *output);
    virtual ~RenderOutput();

    virtual QRect geometry() const;
    virtual AbstractOutput *platformOutput() const;
    QSize pixelSize() const;

    /**
     * Returns a dummy OutputLayer corresponding to the primary plane.
     *
     * TODO: remove this. The Compositor should allocate and deallocate hardware planes
     * after the pre paint pass. Planes must be allocated based on the bounding rect, transform,
     * and visibility (for the cursor plane).
     */
    virtual OutputLayer *layer() const = 0;

    /**
     * TODO replace this with output layers
     */
    virtual bool usesSoftwareCursor() const = 0;

Q_SIGNALS:
    void geometryChanged();

protected:
    AbstractOutput *const m_output;
};

class KWIN_EXPORT SimpleRenderOutput : public RenderOutput
{
public:
    SimpleRenderOutput(AbstractOutput *output);

    bool usesSoftwareCursor() const override;
    OutputLayer *layer() const override;

protected:
    const QScopedPointer<OutputLayer> m_layer;
};
}

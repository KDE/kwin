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

class RenderOutput
{
public:
    RenderOutput(AbstractOutput *output);
    virtual ~RenderOutput() = default;

    virtual QRect geometry() const;
    virtual AbstractOutput *platformOutput() const;

    /**
     * Returns a dummy OutputLayer corresponding to the primary plane.
     *
     * TODO: remove this. The Compositor should allocate and deallocate hardware planes
     * after the pre paint pass. Planes must be allocated based on the bounding rect, transform,
     * and visibility (for the cursor plane).
     */
    OutputLayer *layer() const;
    QSize pixelSize() const;

private:
    const QScopedPointer<OutputLayer> m_layer;
    AbstractOutput *const m_output;
};

}

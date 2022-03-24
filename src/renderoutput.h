/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QRect>
#include <QSharedPointer>

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

    virtual QRect geometry() const = 0;
    virtual AbstractOutput *platformOutput() const;
    QSize pixelSize() const;
    /**
     * @returns the area that this RenderOutput fills within the AbstractOutput it belongs to
     */
    QRect relativePixelGeometry() const;

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
    SimpleRenderOutput(AbstractOutput *output, bool needsSoftwareCursor);

    bool usesSoftwareCursor() const override;
    QRect geometry() const override;

protected:
    const bool m_needsSoftwareCursor;
};
}

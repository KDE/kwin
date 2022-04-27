/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QRect>

#include "outputlayer.h"

namespace KWin
{

class Output;

class KWIN_EXPORT RenderOutput : public QObject
{
    Q_OBJECT

public:
    virtual ~RenderOutput() = default;

    virtual QRect geometry() const = 0;
    virtual Output *platformOutput() const = 0;
    virtual bool usesSoftwareCursor() const;

    QSize pixelSize() const;
    QRect rect() const;

    /**
     * Maps the specified @a rect from the global coordinate system to the output-local coords.
     */
    QRect mapFromGlobal(const QRect &rect) const;

Q_SIGNALS:
    void geometryChanged();
};

class KWIN_EXPORT SimpleRenderOutput : public RenderOutput
{
public:
    SimpleRenderOutput(Output *output);

    QRect geometry() const override;
    Output *platformOutput() const override;
    bool usesSoftwareCursor() const override;

private:
    Output *const m_output;
};

}

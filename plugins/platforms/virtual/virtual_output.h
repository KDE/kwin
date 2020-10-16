/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_VIRTUAL_OUTPUT_H
#define KWIN_VIRTUAL_OUTPUT_H

#include "abstract_wayland_output.h"

#include <QObject>
#include <QRect>

namespace KWin
{
class VirtualBackend;

class VirtualOutput : public AbstractWaylandOutput
{
    Q_OBJECT

public:
    VirtualOutput(QObject *parent = nullptr);
    ~VirtualOutput() override;

    void init(const QPoint &logicalPosition, const QSize &pixelSize);

    void setGeometry(const QRect &geo);

    int gammaRampSize() const override {
        return m_gammaSize;
    }
    bool setGammaRamp(const GammaRamp &gamma) override {
        Q_UNUSED(gamma);
        return m_gammaResult;
    }

private:
    Q_DISABLE_COPY(VirtualOutput);
    friend class VirtualBackend;

    int m_gammaSize = 200;
    bool m_gammaResult = true;
};

}

#endif

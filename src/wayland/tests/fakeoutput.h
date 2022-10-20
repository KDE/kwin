/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"

class FakeOutput : public KWin::Output
{
    Q_OBJECT

public:
    FakeOutput();

    KWin::RenderLoop *renderLoop() const override;
    void setName(const QString &name);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setMode(QSize size, uint32_t refreshRate);
    void setSubPixel(SubPixel subPixel);
    void setDpmsSupported(bool supported);
    void setPhysicalSize(QSize size);
    void setTransform(Transform transform);
    void moveTo(const QPoint &pos);
    void setScale(qreal scale);
};

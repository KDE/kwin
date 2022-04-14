/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPoint>
#include <QSize>

#include "abstract_output.h"
#include "kwin_export.h"

namespace KWin
{

class KWIN_EXPORT OutputChangeSet
{
public:
    bool enabled;
    QPoint pos;
    float scale;
    QSize modeSize;
    uint32_t refreshRate;
    AbstractOutput::Transform transform;
    uint32_t overscan;
    AbstractOutput::RgbRange rgbRange;
    RenderLoop::VrrPolicy vrrPolicy;
};

class KWIN_EXPORT WaylandOutputConfig
{
public:
    QSharedPointer<OutputChangeSet> changeSet(AbstractOutput *output);
    QSharedPointer<OutputChangeSet> constChangeSet(AbstractOutput *output) const;

private:
    QMap<AbstractOutput *, QSharedPointer<OutputChangeSet>> m_properties;
};

}

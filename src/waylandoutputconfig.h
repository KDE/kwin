/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPoint>
#include <QSize>

#include "abstract_wayland_output.h"
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
    AbstractWaylandOutput::Transform transform;
    uint32_t overscan;
    AbstractWaylandOutput::RgbRange rgbRange;
    RenderLoop::VrrPolicy vrrPolicy;
};

class KWIN_EXPORT WaylandOutputConfig
{
public:
    QSharedPointer<OutputChangeSet> changeSet(AbstractWaylandOutput *output);
    QSharedPointer<OutputChangeSet> constChangeSet(AbstractWaylandOutput *output) const;

private:
    QMap<AbstractWaylandOutput*, QSharedPointer<OutputChangeSet>> m_properties;
};

}

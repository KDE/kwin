/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "waylandoutputconfig.h"

namespace KWin
{

QSharedPointer<OutputChangeSet> WaylandOutputConfig::changeSet(AbstractWaylandOutput *output)
{
    const auto ptr = constChangeSet(output);
    m_properties[output] = ptr;
    return ptr;
}

QSharedPointer<OutputChangeSet> WaylandOutputConfig::constChangeSet(AbstractWaylandOutput *output) const
{
    if (!m_properties.contains(output)) {
        auto props = QSharedPointer<OutputChangeSet>::create();
        props->enabled = output->isEnabled();
        props->pos = output->geometry().topLeft();
        props->scale = output->scale();
        props->modeSize = output->modeSize();
        props->refreshRate = output->refreshRate();
        props->transform = output->transform();
        props->overscan = output->overscan();
        props->rgbRange = output->rgbRange();
        props->vrrPolicy = output->vrrPolicy();
        return props;
    }
    return m_properties[output];
}

}

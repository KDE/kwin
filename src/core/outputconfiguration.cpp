/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "outputconfiguration.h"

namespace KWin
{

std::shared_ptr<OutputChangeSet> OutputConfiguration::changeSet(Output *output)
{
    const auto ptr = constChangeSet(output);
    m_properties[output] = ptr;
    return ptr;
}

std::shared_ptr<OutputChangeSet> OutputConfiguration::constChangeSet(Output *output) const
{
    if (!m_properties.contains(output)) {
        auto props = std::make_shared<OutputChangeSet>();
        props->enabled = output->isEnabled();
        props->pos = output->geometry().topLeft();
        props->scale = output->scale();
        props->mode = output->currentMode();
        props->transform = output->transform();
        props->overscan = output->overscan();
        props->rgbRange = output->rgbRange();
        props->vrrPolicy = output->vrrPolicy();
        return props;
    }
    return m_properties[output];
}
}

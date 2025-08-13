/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "outputconfiguration.h"

namespace KWin
{

std::shared_ptr<OutputChangeSet> OutputConfiguration::changeSet(BackendOutput *output)
{
    auto &ret = m_properties[output];
    if (!ret) {
        ret = std::make_shared<OutputChangeSet>();
    }
    return ret;
}

std::shared_ptr<OutputChangeSet> OutputConfiguration::constChangeSet(BackendOutput *output) const
{
    return m_properties[output];
}
}

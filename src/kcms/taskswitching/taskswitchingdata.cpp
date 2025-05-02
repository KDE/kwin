/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "taskswitchingdata.h"

namespace KWin
{

TaskSwitchingData::TaskSwitchingData(QObject *parent)
    : KCModuleData(parent)
    , m_tabBoxSettings(new TabBoxSettings(QStringLiteral("TabBox"), this))
    , m_tabBoxAlternativeSettings(new TabBoxSettings(QStringLiteral("TabBoxAlternative"), this))
{
    registerSkeleton(m_tabBoxSettings);
    registerSkeleton(m_tabBoxAlternativeSettings);
}

TaskSwitchingData::~TaskSwitchingData()
{
}

TabBoxSettings *TaskSwitchingData::tabBoxSettings() const
{
    return m_tabBoxSettings;
}

TabBoxSettings *TaskSwitchingData::tabBoxAlternativeSettings() const
{
    return m_tabBoxAlternativeSettings;
}

} // namespace KWin

#include "moc_taskswitchingdata.cpp"

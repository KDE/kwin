/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <KCModuleData>

#include "overviewsettings.h"
#include "tabboxsettings.h"

namespace KWin
{

class TaskSwitchingData : public KCModuleData
{
    Q_OBJECT

public:
    explicit TaskSwitchingData(QObject *parent);
    ~TaskSwitchingData() override;

    TabBoxSettings *tabBoxSettings() const;
    TabBoxSettings *tabBoxAlternativeSettings() const;
    OverviewSettings *overviewSettings() const;

private:
    TabBoxSettings *m_tabBoxSettings;
    TabBoxSettings *m_tabBoxAlternativeSettings;
    OverviewSettings *m_overviewSettings;
};

} // namespace KWin

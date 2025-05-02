/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

#include <KCModuleData>

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

private:
    TabBoxSettings *m_tabBoxSettings;
    TabBoxSettings *m_tabBoxAlternativeSettings;
};

} // namespace KWin

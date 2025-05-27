/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QProcess>

#include <KQuickManagedConfigModule>

#include "tabboxlayoutsmodel.h"
#include "taskswitchingdata.h"

namespace KWin
{

class TaskSwitchingKCM : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(TabBoxSettings *tabBoxSettings READ tabBoxSettings CONSTANT)
    Q_PROPERTY(TabBoxSettings *tabBoxAlternativeSettings READ tabBoxAlternativeSettings CONSTANT)
    Q_PROPERTY(TabBoxLayoutsModel *tabBoxLayouts READ tabBoxLayouts CONSTANT)
    Q_PROPERTY(OverviewSettings *overviewSettings READ overviewSettings CONSTANT)

public:
    TaskSwitchingKCM(QObject *parent, const KPluginMetaData &metaData);
    ~TaskSwitchingKCM() override;

    TabBoxSettings *tabBoxSettings() const;
    TabBoxSettings *tabBoxAlternativeSettings() const;
    TabBoxLayoutsModel *tabBoxLayouts() const;
    OverviewSettings *overviewSettings() const;

    bool isDefaults() const override;
    void save() override;

    Q_INVOKABLE void configureTabBoxShortcuts(const bool showAlternative);
    Q_INVOKABLE void configureOverviewShortcuts();
    Q_INVOKABLE void ghnsEntryChanged();

private:
    TaskSwitchingData *m_data;
    TabBoxLayoutsModel *m_tabBoxLayouts;
    std::unique_ptr<QProcess> m_tabBoxLayoutPreviewProcess;

    Q_DISABLE_COPY(TaskSwitchingKCM)
};

} // namespace KWin

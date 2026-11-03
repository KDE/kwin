/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
    Q_PROPERTY(QAbstractItemModel *tabBoxLayouts READ tabBoxLayouts CONSTANT)
    Q_PROPERTY(OverviewSettings *overviewSettings READ overviewSettings CONSTANT)
    Q_PROPERTY(QStringList overviewShortcuts READ overviewShortcuts NOTIFY overviewShortcutsChanged)
    Q_PROPERTY(bool tabBoxLimitedByFocusPolicy READ tabBoxLimitedByFocusPolicy CONSTANT)

public:
    TaskSwitchingKCM(QObject *parent, const KPluginMetaData &metaData);
    ~TaskSwitchingKCM() override;

    TabBoxSettings *tabBoxSettings() const;
    TabBoxSettings *tabBoxAlternativeSettings() const;
    QAbstractItemModel *tabBoxLayouts() const;
    OverviewSettings *overviewSettings() const;
    QStringList overviewShortcuts() const;
    bool tabBoxLimitedByFocusPolicy() const;

    bool isDefaults() const override;
    void save() override;

    Q_INVOKABLE void configureTabBoxShortcuts(const bool showAlternative, QQuickItem *context);
    Q_INVOKABLE void configureOverviewShortcuts(QQuickItem *context);
    Q_INVOKABLE void ghnsEntryChanged();
    Q_INVOKABLE void showPreview(const QString &pluginId, bool showDesktop);

Q_SIGNALS:
    void overviewShortcutsChanged();

private:
    const TaskSwitchingData *const m_data;
    TabBoxLayoutsModel *m_tabBoxLayouts;
    TabBoxLayoutsProxyModel *m_tabBoxLayoutsProxy;
};

} // namespace KWin

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef __KWINTABBOXCONFIGFORM_H__
#define __KWINTABBOXCONFIGFORM_H__

#include <QWidget>
#include <QStandardItemModel>

#include "tabboxconfig.h"

class KShortcutsEditor;
class KActionCollection;

namespace Ui
{
class KWinTabBoxConfigForm;
}

namespace KWin
{

class KWinTabBoxConfigForm : public QWidget
{
    Q_OBJECT

public:
    enum class TabboxType
    {
        Main,
        Alternative,
    };


    enum EffectComboRole
    {
        LayoutPath = Qt::UserRole + 1,
        AddonEffect, // i.e not builtin effects
    };

    explicit KWinTabBoxConfigForm(TabboxType type, QWidget *parent = nullptr);
    ~KWinTabBoxConfigForm() override;

    bool highlightWindows() const;
    bool showTabBox() const;
    int filterScreen() const;
    int filterDesktop() const;
    int filterActivities() const;
    int filterMinimization() const;
    int applicationMode() const;
    int showDesktopMode() const;
    int switchingMode() const;
    QString layoutName() const;

    void setFilterScreen(TabBox::TabBoxConfig::ClientMultiScreenMode mode);
    void setFilterDesktop(TabBox::TabBoxConfig::ClientDesktopMode mode);
    void setFilterActivities(TabBox::TabBoxConfig::ClientActivitiesMode mode);
    void setFilterMinimization(TabBox::TabBoxConfig::ClientMinimizedMode mode);
    void setApplicationMode(TabBox::TabBoxConfig::ClientApplicationsMode mode);
    void setShowDesktopMode(TabBox::TabBoxConfig::ShowDesktopMode mode);
    void setSwitchingModeChanged(TabBox::TabBoxConfig::ClientSwitchingMode mode);
    void setLayoutName(const QString &layoutName);

    // EffectCombo Data Model
    void setEffectComboModel(QStandardItemModel *model);
    QVariant effectComboCurrentData(int role = Qt::UserRole) const;

    void loadShortcuts();
    void resetShortcuts();

    void setHighlightWindowsEnabled(bool enabled);
    void setFilterScreenEnabled(bool enabled);
    void setFilterDesktopEnabled(bool enabled);
    void setFilterActivitiesEnabled(bool enabled);
    void setFilterMinimizationEnabled(bool enabled);
    void setApplicationModeEnabled(bool enabled);
    void setShowDesktopModeEnabled(bool enabled);
    void setSwitchingModeEnabled(bool enabled);
    void setLayoutNameEnabled(bool enabled);

Q_SIGNALS:
    void filterScreenChanged(int value);
    void filterDesktopChanged(int value);
    void filterActivitiesChanged(int value);
    void filterMinimizationChanged(int value);
    void applicationModeChanged(int value);
    void showDesktopModeChanged(int value);
    void switchingModeChanged(int value);
    void layoutNameChanged(const QString &layoutName);
    void effectConfigButtonClicked();

private Q_SLOTS:
    void tabBoxToggled(bool on);
    void onFilterScreen();
    void onFilterDesktop();
    void onFilterActivites();
    void onFilterMinimization();
    void onApplicationMode();
    void onShowDesktopMode();
    void onSwitchingMode();
    void onEffectCombo();
    void shortcutChanged(const QKeySequence &seq);

private:
    KActionCollection *m_actionCollection = nullptr;
    KShortcutsEditor *m_editor = nullptr;

    bool m_isHighlightWindowsEnabled = true;
    TabboxType m_type;
    Ui::KWinTabBoxConfigForm *ui;
};

} // namespace

#endif

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>
    SPDX-FileCopyrightText: 2023 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QStandardItemModel>
#include <QWidget>

#include "tabbox/tabboxconfig.h"

namespace Ui
{
class KWinTabBoxConfigForm;
}

namespace KWin
{

namespace TabBox
{
class TabBoxSettings;
class ShortcutSettings;
}

class KWinTabBoxConfigForm : public QWidget
{
    Q_OBJECT

public:
    enum class TabboxType {
        Main,
        Alternative,
    };

    enum EffectComboRole {
        LayoutPath = Qt::UserRole + 1,
        AddonEffect, // i.e not builtin effects
    };

    explicit KWinTabBoxConfigForm(TabboxType type, TabBox::TabBoxSettings *config, TabBox::ShortcutSettings *shortcutsConfig, QWidget *parent = nullptr);
    ~KWinTabBoxConfigForm() override;

    TabBox::TabBoxSettings *config() const;
    bool highlightWindows() const;

    void updateUiFromConfig();
    void setDefaultIndicatorVisible(bool visible);

    // EffectCombo Data Model
    void setEffectComboModel(QStandardItemModel *model);
    QVariant effectComboCurrentData(int role = Qt::UserRole) const;

Q_SIGNALS:
    void configChanged();
    void effectConfigButtonClicked();

private Q_SLOTS:
    void tabBoxToggled(bool on);
    void onFilterScreen();
    void onFilterDesktop();
    void onFilterActivites();
    void onFilterMinimization();
    void onApplicationMode();
    void onOrderMinimizedMode();
    void onShowDesktopMode();
    void onSwitchingMode();
    void onEffectCombo();
    void updateDefaultIndicators();

private:
    void setEnabledUi();
    void applyDefaultIndicator(QList<QWidget *> widgets, bool visible);

    // UI property getters
    bool showTabBox() const;
    int filterScreen() const;
    int filterDesktop() const;
    int filterActivities() const;
    int filterMinimization() const;
    int applicationMode() const;
    int orderMinimizedMode() const;
    int showDesktopMode() const;
    int switchingMode() const;
    QString layoutName() const;

    // UI property setters
    void setFilterScreen(TabBox::TabBoxConfig::ClientMultiScreenMode mode);
    void setFilterDesktop(TabBox::TabBoxConfig::ClientDesktopMode mode);
    void setFilterActivities(TabBox::TabBoxConfig::ClientActivitiesMode mode);
    void setFilterMinimization(TabBox::TabBoxConfig::ClientMinimizedMode mode);
    void setApplicationMode(TabBox::TabBoxConfig::ClientApplicationsMode mode);
    void setOrderMinimizedMode(TabBox::TabBoxConfig::OrderMinimizedMode mode);
    void setShowDesktopMode(TabBox::TabBoxConfig::ShowDesktopMode mode);
    void setSwitchingModeChanged(TabBox::TabBoxConfig::ClientSwitchingMode mode);
    void setLayoutName(const QString &layoutName);

private:
    TabBox::TabBoxSettings *m_config = nullptr;
    TabBox::ShortcutSettings *m_shortcuts = nullptr;
    bool m_showDefaultIndicator = false;

    bool m_isHighlightWindowsEnabled = true;
    Ui::KWinTabBoxConfigForm *ui;
};

} // namespace

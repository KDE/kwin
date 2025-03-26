/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>
    SPDX-FileCopyrightText: 2023 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwintabboxconfigform.h"
#include "kwintabboxsettings.h"
#include "shortcutsettings.h"
#include "ui_main.h"

namespace KWin
{

using namespace TabBox;

KWinTabBoxConfigForm::KWinTabBoxConfigForm(TabboxType type, TabBoxSettings *config, ShortcutSettings *shortcutsConfig, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
    , m_shortcuts(shortcutsConfig)
    , ui(new Ui::KWinTabBoxConfigForm)
{
    ui->setupUi(this);

    if (QApplication::screens().count() < 2) {
        ui->filterScreens->hide();
        ui->screenFilter->hide();
    }

    connect(this, &KWinTabBoxConfigForm::configChanged, this, &KWinTabBoxConfigForm::updateDefaultIndicators);

    connect(ui->effectConfigButton, &QPushButton::clicked, this, &KWinTabBoxConfigForm::effectConfigButtonClicked);

    connect(ui->kcfg_ShowTabBox, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::tabBoxToggled);

    connect(ui->filterScreens, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterScreen);
    connect(ui->currentScreen, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterScreen);
    connect(ui->otherScreens, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterScreen);

    connect(ui->filterDesktops, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterDesktop);
    connect(ui->currentDesktop, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterDesktop);
    connect(ui->otherDesktops, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterDesktop);

    connect(ui->filterActivities, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterActivites);
    connect(ui->currentActivity, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterActivites);
    connect(ui->otherActivities, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterActivites);

    connect(ui->filterMinimization, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterMinimization);
    connect(ui->visibleWindows, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterMinimization);
    connect(ui->hiddenWindows, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onFilterMinimization);

    connect(ui->oneAppWindow, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onApplicationMode);
    connect(ui->orderMinimized, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onOrderMinimizedMode);
    connect(ui->showDesktop, &QAbstractButton::clicked, this, &KWinTabBoxConfigForm::onShowDesktopMode);

    connect(ui->switchingModeCombo, &QComboBox::activated, this, &KWinTabBoxConfigForm::onSwitchingMode);
    connect(ui->effectCombo, &QComboBox::activated, this, &KWinTabBoxConfigForm::onEffectCombo);

    auto initShortcutWidget = [this](KKeySequenceWidget *primary, KKeySequenceWidget *alternate, const QString &name) {
        primary->setCheckActionCollections({m_shortcuts->actionCollection()});
        primary->setProperty("shortcutAction", name);
        connect(primary, &KKeySequenceWidget::keySequenceChanged, this, [this, name](const QKeySequence &seq) {
            if (m_shortcuts->primaryShortcut(name) != seq) {
                m_shortcuts->setShortcuts(name, {seq, m_shortcuts->alternateShortcut(name)});
                Q_EMIT configChanged();
            }
        });

        alternate->setCheckActionCollections({m_shortcuts->actionCollection()});
        alternate->setProperty("shortcutAction", name);
        connect(alternate, &KKeySequenceWidget::keySequenceChanged, this, [this, name](const QKeySequence &seq) {
            if (m_shortcuts->alternateShortcut(name) != seq) {
                m_shortcuts->setShortcuts(name, {m_shortcuts->primaryShortcut(name), seq});
                Q_EMIT configChanged();
            }
        });
    };

    if (TabboxType::Main == type) {
        initShortcutWidget(ui->scAll, ui->scAllAlternate, QStringLiteral("Walk Through Windows"));
        initShortcutWidget(ui->scAllReverse, ui->scAllReverseAlternate, QStringLiteral("Walk Through Windows (Reverse)"));
        initShortcutWidget(ui->scCurrent, ui->scCurrentAlternate, QStringLiteral("Walk Through Windows of Current Application"));
        initShortcutWidget(ui->scCurrentReverse, ui->scCurrentReverseAlternate, QStringLiteral("Walk Through Windows of Current Application (Reverse)"));
    } else if (TabboxType::Alternative == type) {
        initShortcutWidget(ui->scAll, ui->scAllAlternate, QStringLiteral("Walk Through Windows Alternative"));
        initShortcutWidget(ui->scAllReverse, ui->scAllReverseAlternate, QStringLiteral("Walk Through Windows Alternative (Reverse)"));
        initShortcutWidget(ui->scCurrent, ui->scCurrentAlternate, QStringLiteral("Walk Through Windows of Current Application Alternative"));
        initShortcutWidget(ui->scCurrentReverse, ui->scCurrentReverseAlternate, QStringLiteral("Walk Through Windows of Current Application Alternative (Reverse)"));
    }

    updateUiFromConfig();
}

KWinTabBoxConfigForm::~KWinTabBoxConfigForm()
{
    delete ui;
}

TabBoxSettings *KWinTabBoxConfigForm::config() const
{
    return m_config;
}

bool KWinTabBoxConfigForm::highlightWindows() const
{
    return ui->kcfg_HighlightWindows->isChecked();
}

bool KWinTabBoxConfigForm::showTabBox() const
{
    return ui->kcfg_ShowTabBox->isChecked();
}

int KWinTabBoxConfigForm::filterScreen() const
{
    if (ui->filterScreens->isChecked()) {
        return ui->currentScreen->isChecked() ? TabBoxConfig::OnlyCurrentScreenClients : TabBoxConfig::ExcludeCurrentScreenClients;
    } else {
        return TabBoxConfig::IgnoreMultiScreen;
    }
}

int KWinTabBoxConfigForm::filterDesktop() const
{
    if (ui->filterDesktops->isChecked()) {
        return ui->currentDesktop->isChecked() ? TabBoxConfig::OnlyCurrentDesktopClients : TabBoxConfig::ExcludeCurrentDesktopClients;
    } else {
        return TabBoxConfig::AllDesktopsClients;
    }
}

int KWinTabBoxConfigForm::filterActivities() const
{
    if (ui->filterActivities->isChecked()) {
        return ui->currentActivity->isChecked() ? TabBoxConfig::OnlyCurrentActivityClients : TabBoxConfig::ExcludeCurrentActivityClients;
    } else {
        return TabBoxConfig::AllActivitiesClients;
    }
}

int KWinTabBoxConfigForm::filterMinimization() const
{
    if (ui->filterMinimization->isChecked()) {
        return ui->visibleWindows->isChecked() ? TabBoxConfig::ExcludeMinimizedClients : TabBoxConfig::OnlyMinimizedClients;
    } else {
        return TabBoxConfig::IgnoreMinimizedStatus;
    }
}

int KWinTabBoxConfigForm::applicationMode() const
{
    return ui->oneAppWindow->isChecked() ? TabBoxConfig::OneWindowPerApplication : TabBoxConfig::AllWindowsAllApplications;
}

int KWinTabBoxConfigForm::orderMinimizedMode() const
{
    return ui->orderMinimized->isChecked() ? TabBoxConfig::GroupByMinimized : TabBoxConfig::NoGroupByMinimized;
}

int KWinTabBoxConfigForm::showDesktopMode() const
{
    return ui->showDesktop->isChecked() ? TabBoxConfig::ShowDesktopClient : TabBoxConfig::DoNotShowDesktopClient;
}

int KWinTabBoxConfigForm::switchingMode() const
{
    return ui->switchingModeCombo->currentIndex();
}

QString KWinTabBoxConfigForm::layoutName() const
{
    return ui->effectCombo->currentData().toString();
}

void KWinTabBoxConfigForm::setFilterScreen(TabBox::TabBoxConfig::ClientMultiScreenMode mode)
{
    ui->filterScreens->setChecked(mode != TabBoxConfig::IgnoreMultiScreen);
    ui->currentScreen->setChecked(mode == TabBoxConfig::OnlyCurrentScreenClients);
    ui->otherScreens->setChecked(mode == TabBoxConfig::ExcludeCurrentScreenClients);
}

void KWinTabBoxConfigForm::setFilterDesktop(TabBox::TabBoxConfig::ClientDesktopMode mode)
{
    ui->filterDesktops->setChecked(mode != TabBoxConfig::AllDesktopsClients);
    ui->currentDesktop->setChecked(mode == TabBoxConfig::OnlyCurrentDesktopClients);
    ui->otherDesktops->setChecked(mode == TabBoxConfig::ExcludeCurrentDesktopClients);
}

void KWinTabBoxConfigForm::setFilterActivities(TabBox::TabBoxConfig::ClientActivitiesMode mode)
{
    ui->filterActivities->setChecked(mode != TabBoxConfig::AllActivitiesClients);
    ui->currentActivity->setChecked(mode == TabBoxConfig::OnlyCurrentActivityClients);
    ui->otherActivities->setChecked(mode == TabBoxConfig::ExcludeCurrentActivityClients);
}

void KWinTabBoxConfigForm::setFilterMinimization(TabBox::TabBoxConfig::ClientMinimizedMode mode)
{
    ui->filterMinimization->setChecked(mode != TabBoxConfig::IgnoreMinimizedStatus);
    ui->visibleWindows->setChecked(mode == TabBoxConfig::ExcludeMinimizedClients);
    ui->hiddenWindows->setChecked(mode == TabBoxConfig::OnlyMinimizedClients);
}

void KWinTabBoxConfigForm::setApplicationMode(TabBox::TabBoxConfig::ClientApplicationsMode mode)
{
    ui->oneAppWindow->setChecked(mode == TabBoxConfig::OneWindowPerApplication);
}

void KWinTabBoxConfigForm::setOrderMinimizedMode(TabBox::TabBoxConfig::OrderMinimizedMode mode)
{
    ui->orderMinimized->setChecked(mode == TabBoxConfig::GroupByMinimized);
}

void KWinTabBoxConfigForm::setShowDesktopMode(TabBox::TabBoxConfig::ShowDesktopMode mode)
{
    ui->showDesktop->setChecked(mode == TabBoxConfig::ShowDesktopClient);
}

void KWinTabBoxConfigForm::setSwitchingModeChanged(TabBox::TabBoxConfig::ClientSwitchingMode mode)
{
    ui->switchingModeCombo->setCurrentIndex(mode);
}

void KWinTabBoxConfigForm::setLayoutName(const QString &layoutName)
{
    const int index = ui->effectCombo->findData(layoutName);
    if (index >= 0) {
        ui->effectCombo->setCurrentIndex(index);
    }
}

void KWinTabBoxConfigForm::setEffectComboModel(QStandardItemModel *model)
{
    // We don't want to lose the config layout when resetting the combo model
    const QString layout = m_config->layoutName();
    ui->effectCombo->setModel(model);
    setLayoutName(layout);
}

QVariant KWinTabBoxConfigForm::effectComboCurrentData(int role) const
{
    return ui->effectCombo->currentData(role);
}

void KWinTabBoxConfigForm::tabBoxToggled(bool on)
{
    // Highlight Windows options is availabled if no TabBox effect is selected
    // or if Tabbox is not builtin effet.
    on = !on || ui->effectCombo->currentData(AddonEffect).toBool();
    ui->kcfg_HighlightWindows->setEnabled(on && m_isHighlightWindowsEnabled);
}

void KWinTabBoxConfigForm::onFilterScreen()
{
    m_config->setMultiScreenMode(filterScreen());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::onFilterDesktop()
{
    m_config->setDesktopMode(filterDesktop());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::onFilterActivites()
{
    m_config->setActivitiesMode(filterActivities());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::onFilterMinimization()
{
    m_config->setMinimizedMode(filterMinimization());
    Q_EMIT configChanged();
}

void KWin::KWinTabBoxConfigForm::onApplicationMode()
{
    m_config->setApplicationsMode(applicationMode());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::onOrderMinimizedMode()
{
    m_config->setOrderMinimizedMode(orderMinimizedMode());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::onShowDesktopMode()
{
    m_config->setShowDesktopMode(showDesktopMode());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::onSwitchingMode()
{
    m_config->setSwitchingMode(switchingMode());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::onEffectCombo()
{
    const bool isAddonEffect = ui->effectCombo->currentData(AddonEffect).toBool();
    ui->effectConfigButton->setIcon(QIcon::fromTheme(isAddonEffect ? "view-preview" : "configure"));
    if (!ui->kcfg_ShowTabBox->isChecked()) {
        return;
    }
    ui->kcfg_HighlightWindows->setEnabled(isAddonEffect && m_isHighlightWindowsEnabled);

    m_config->setLayoutName(layoutName());
    Q_EMIT configChanged();
}

void KWinTabBoxConfigForm::updateUiFromConfig()
{
    setFilterScreen(static_cast<TabBoxConfig::ClientMultiScreenMode>(m_config->multiScreenMode()));
    setFilterDesktop(static_cast<TabBoxConfig::ClientDesktopMode>(m_config->desktopMode()));
    setFilterActivities(static_cast<TabBoxConfig::ClientActivitiesMode>(m_config->activitiesMode()));
    setFilterMinimization(static_cast<TabBoxConfig::ClientMinimizedMode>(m_config->minimizedMode()));
    setApplicationMode(static_cast<TabBoxConfig::ClientApplicationsMode>(m_config->applicationsMode()));
    setOrderMinimizedMode(static_cast<TabBoxConfig::OrderMinimizedMode>(m_config->orderMinimizedMode()));
    setShowDesktopMode(static_cast<TabBoxConfig::ShowDesktopMode>(m_config->showDesktopMode()));
    setSwitchingModeChanged(static_cast<TabBoxConfig::ClientSwitchingMode>(m_config->switchingMode()));
    setLayoutName(m_config->layoutName());

    for (const auto &widget : {ui->scAll, ui->scAllReverse, ui->scCurrent, ui->scCurrentReverse}) {
        const QString actionName = widget->property("shortcutAction").toString();
        widget->setKeySequence(m_shortcuts->primaryShortcut(actionName));
    }
    for (const auto &widget : {ui->scAllAlternate, ui->scAllReverseAlternate, ui->scCurrentAlternate, ui->scCurrentReverseAlternate}) {
        const QString actionName = widget->property("shortcutAction").toString();
        widget->setKeySequence(m_shortcuts->alternateShortcut(actionName));
    }

    updateDefaultIndicators();
}

void KWinTabBoxConfigForm::setEnabledUi()
{
    m_isHighlightWindowsEnabled = !m_config->isHighlightWindowsImmutable();
    ui->kcfg_HighlightWindows->setEnabled(!m_config->isHighlightWindowsImmutable());

    ui->filterScreens->setEnabled(!m_config->isMultiScreenModeImmutable());
    ui->currentScreen->setEnabled(!m_config->isMultiScreenModeImmutable());
    ui->otherScreens->setEnabled(!m_config->isMultiScreenModeImmutable());

    ui->filterDesktops->setEnabled(!m_config->isDesktopModeImmutable());
    ui->currentDesktop->setEnabled(!m_config->isDesktopModeImmutable());
    ui->otherDesktops->setEnabled(!m_config->isDesktopModeImmutable());

    ui->filterActivities->setEnabled(!m_config->isActivitiesModeImmutable());
    ui->currentActivity->setEnabled(!m_config->isActivitiesModeImmutable());
    ui->otherActivities->setEnabled(!m_config->isActivitiesModeImmutable());

    ui->filterMinimization->setEnabled(!m_config->isMinimizedModeImmutable());
    ui->visibleWindows->setEnabled(!m_config->isMinimizedModeImmutable());
    ui->hiddenWindows->setEnabled(!m_config->isMinimizedModeImmutable());

    ui->oneAppWindow->setEnabled(!m_config->isApplicationsModeImmutable());
    ui->orderMinimized->setEnabled(!m_config->isOrderMinimizedModeImmutable());
    ui->showDesktop->setEnabled(!m_config->isShowDesktopModeImmutable());
    ui->switchingModeCombo->setEnabled(!m_config->isSwitchingModeImmutable());
    ui->effectCombo->setEnabled(!m_config->isLayoutNameImmutable());
}

void KWinTabBoxConfigForm::setDefaultIndicatorVisible(bool show)
{
    m_showDefaultIndicator = show;
    updateDefaultIndicators();
}

void KWinTabBoxConfigForm::updateDefaultIndicators()
{
    applyDefaultIndicator({ui->filterScreens, ui->currentScreen, ui->otherScreens},
                          m_config->multiScreenMode() == m_config->defaultMultiScreenModeValue());
    applyDefaultIndicator({ui->filterDesktops, ui->currentDesktop, ui->otherDesktops},
                          m_config->desktopMode() == m_config->defaultDesktopModeValue());
    applyDefaultIndicator({ui->filterActivities, ui->currentActivity, ui->otherActivities},
                          m_config->activitiesMode() == m_config->defaultActivitiesModeValue());
    applyDefaultIndicator({ui->filterMinimization, ui->visibleWindows, ui->hiddenWindows},
                          m_config->minimizedMode() == m_config->defaultMinimizedModeValue());
    applyDefaultIndicator({ui->oneAppWindow}, m_config->applicationsMode() == m_config->defaultApplicationsModeValue());
    applyDefaultIndicator({ui->orderMinimized}, m_config->orderMinimizedMode() == m_config->defaultOrderMinimizedModeValue());
    applyDefaultIndicator({ui->showDesktop}, m_config->showDesktopMode() == m_config->defaultShowDesktopModeValue());
    applyDefaultIndicator({ui->switchingModeCombo}, m_config->switchingMode() == m_config->defaultSwitchingModeValue());
    applyDefaultIndicator({ui->effectCombo}, m_config->layoutName() == m_config->defaultLayoutNameValue());

    for (const auto &widget : {ui->scAll, ui->scAllAlternate, ui->scAllReverse, ui->scAllReverseAlternate, ui->scCurrent, ui->scCurrentAlternate, ui->scCurrentReverse, ui->scCurrentReverseAlternate}) {
        const QString actionName = widget->property("shortcutAction").toString();
        applyDefaultIndicator({widget}, m_shortcuts->isDefault(actionName));
    }
}

void KWinTabBoxConfigForm::applyDefaultIndicator(QList<QWidget *> widgets, bool isDefault)
{
    for (auto widget : widgets) {
        widget->setProperty("_kde_highlight_neutral", m_showDefaultIndicator && !isDefault);
        widget->update();
    }
}

} // namespace

#include "moc_kwintabboxconfigform.cpp"

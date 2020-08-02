/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwintabboxconfigform.h"
#include "ui_main.h"

#include <QDebug>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KShortcutsEditor>


namespace KWin
{

using namespace TabBox;

KWinTabBoxConfigForm::KWinTabBoxConfigForm(TabboxType type, QWidget *parent)
    : QWidget(parent)
    , m_type(type)
    , ui(new Ui::KWinTabBoxConfigForm)
{
    ui->setupUi(this);

    ui->effectConfigButton->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));

    if (QApplication::screens().count() < 2) {
        ui->filterScreens->hide();
        ui->screenFilter->hide();
    }

    connect(ui->effectConfigButton, &QPushButton::clicked, this, &KWinTabBoxConfigForm::effectConfigButtonClicked);

    connect(ui->kcfg_ShowTabBox, SIGNAL(clicked(bool)), SLOT(tabBoxToggled(bool)));

    connect(ui->filterScreens, SIGNAL(clicked(bool)), SLOT(onFilterScreen()));
    connect(ui->currentScreen, SIGNAL(clicked(bool)), SLOT(onFilterScreen()));
    connect(ui->otherScreens, SIGNAL(clicked(bool)), SLOT(onFilterScreen()));

    connect(ui->filterDesktops, SIGNAL(clicked(bool)), SLOT(onFilterDesktop()));
    connect(ui->currentDesktop, SIGNAL(clicked(bool)), SLOT(onFilterDesktop()));
    connect(ui->otherDesktops, SIGNAL(clicked(bool)), SLOT(onFilterDesktop()));

    connect(ui->filterActivities, SIGNAL(clicked(bool)), SLOT(onFilterActivites()));
    connect(ui->currentActivity, SIGNAL(clicked(bool)), SLOT(onFilterActivites()));
    connect(ui->otherActivities, SIGNAL(clicked(bool)), SLOT(onFilterActivites()));

    connect(ui->filterMinimization, SIGNAL(clicked(bool)), SLOT(onFilterMinimization()));
    connect(ui->visibleWindows, SIGNAL(clicked(bool)), SLOT(onFilterMinimization()));
    connect(ui->hiddenWindows, SIGNAL(clicked(bool)), SLOT(onFilterMinimization()));

    connect(ui->oneAppWindow, SIGNAL(clicked(bool)), SLOT(onApplicationMode()));
    connect(ui->showDesktop, SIGNAL(clicked(bool)), SLOT(onShowDesktopMode()));

    connect(ui->switchingModeCombo, SIGNAL(currentIndexChanged(int)), SLOT(onSwitchingMode()));
    connect(ui->effectCombo, SIGNAL(currentIndexChanged(int)), SLOT(onEffectCombo()));

    auto addShortcut = [this](const char *name, KKeySequenceWidget *widget, const QKeySequence &sequence = QKeySequence()) {
        QAction *a = m_actionCollection->addAction(name);
        a->setProperty("isConfigurationAction", true);
        widget->setProperty("shortcutAction", name);
        a->setText(i18n(name));
        KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << sequence);
        connect(widget, SIGNAL(keySequenceChanged(QKeySequence)), this, SLOT(shortcutChanged(QKeySequence)));
    };

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup("Navigation");
    m_actionCollection->setConfigGlobal(true);

    if (TabboxType::Main == m_type) {
        addShortcut("Walk Through Windows", ui->scAll, Qt::ALT + Qt::Key_Tab);
        addShortcut("Walk Through Windows (Reverse)", ui->scAllReverse, Qt::ALT + Qt::SHIFT + Qt::Key_Backtab);
        addShortcut("Walk Through Windows of Current Application", ui->scCurrent, Qt::ALT + Qt::Key_QuoteLeft);
        addShortcut("Walk Through Windows of Current Application (Reverse)", ui->scCurrentReverse, Qt::ALT + Qt::Key_AsciiTilde);
    } else if (TabboxType::Alternative == m_type) {
        addShortcut("Walk Through Windows Alternative", ui->scAll);
        addShortcut("Walk Through Windows Alternative (Reverse)", ui->scAllReverse);
        addShortcut("Walk Through Windows of Current Application Alternative", ui->scCurrent);
        addShortcut("Walk Through Windows of Current Application Alternative (Reverse)", ui->scCurrentReverse);
    }
}

KWinTabBoxConfigForm::~KWinTabBoxConfigForm()
{
    delete ui;
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
    ui->effectCombo->setCurrentIndex(ui->effectCombo->findData(layoutName));
}

void KWinTabBoxConfigForm::setEffectComboModel(QStandardItemModel *model)
{
    int index = ui->effectCombo->currentIndex();
    QVariant data = ui->effectCombo->itemData(index);

    ui->effectCombo->setModel(model);

    if (data.isValid()) {
        ui->effectCombo->setCurrentIndex(ui->effectCombo->findData(data));
    } else if (index != -1) {
        ui->effectCombo->setCurrentIndex(index);
    }
}

QVariant KWinTabBoxConfigForm::effectComboCurrentData(int role) const
{
    return ui->effectCombo->currentData(role);
}

void KWinTabBoxConfigForm::loadShortcuts()
{
    auto loadShortcut = [this](KKeySequenceWidget *widget) {
        QString actionName = widget->property("shortcutAction").toString();
        qDebug() << "load shortcut for " << actionName;
        if (QAction *action = m_actionCollection->action(actionName)) {
            auto shortcuts = KGlobalAccel::self()->shortcut(action);
            if (!shortcuts.isEmpty()) {
                widget->setKeySequence(shortcuts.first());
            }
        }
    };

    loadShortcut(ui->scAll);
    loadShortcut(ui->scAllReverse);
    loadShortcut(ui->scCurrent);
    loadShortcut(ui->scCurrentReverse);
}

void KWinTabBoxConfigForm::resetShortcuts()
{
    QString action;
    auto resetShortcut = [this](KKeySequenceWidget *widget, const QKeySequence &sequence = QKeySequence()) {
        const QString action = widget->property("shortcutAction").toString();
        QAction *a = m_actionCollection->action(action);
        KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << sequence, KGlobalAccel::NoAutoloading);
    };
    if (TabboxType::Main == m_type) {
        resetShortcut(ui->scAll, Qt::ALT + Qt::Key_Tab);
        resetShortcut(ui->scAllReverse, Qt::ALT + Qt::SHIFT + Qt::Key_Backtab);
        resetShortcut(ui->scCurrent, Qt::ALT + Qt::Key_QuoteLeft);
        resetShortcut(ui->scCurrentReverse, Qt::ALT + Qt::Key_AsciiTilde);
    } else if (TabboxType::Alternative == m_type) {
        resetShortcut(ui->scAll);
        resetShortcut(ui->scAllReverse);
        resetShortcut(ui->scCurrent);
        resetShortcut(ui->scCurrentReverse);
    }
    m_actionCollection->writeSettings();
}

void KWinTabBoxConfigForm::setHighlightWindowsEnabled(bool enabled)
{
    m_isHighlightWindowsEnabled = enabled;
    ui->kcfg_HighlightWindows->setEnabled(m_isHighlightWindowsEnabled);
}

void KWinTabBoxConfigForm::setFilterScreenEnabled(bool enabled)
{
    ui->filterScreens->setEnabled(enabled);
    ui->currentScreen->setEnabled(enabled);
    ui->otherScreens->setEnabled(enabled);
}

void KWinTabBoxConfigForm::setFilterDesktopEnabled(bool enabled)
{
    ui->filterDesktops->setEnabled(enabled);
    ui->currentDesktop->setEnabled(enabled);
    ui->otherDesktops->setEnabled(enabled);
}

void KWinTabBoxConfigForm::setFilterActivitiesEnabled(bool enabled)
{
    ui->filterActivities->setEnabled(enabled);
    ui->currentActivity->setEnabled(enabled);
    ui->otherActivities->setEnabled(enabled);
}

void KWinTabBoxConfigForm::setFilterMinimizationEnabled(bool enabled)
{
    ui->filterMinimization->setEnabled(enabled);
    ui->visibleWindows->setEnabled(enabled);
    ui->hiddenWindows->setEnabled(enabled);
}

void KWinTabBoxConfigForm::setApplicationModeEnabled(bool enabled)
{
    ui->oneAppWindow->setEnabled(enabled);
}

void KWinTabBoxConfigForm::setShowDesktopModeEnabled(bool enabled)
{
    ui->showDesktop->setEnabled(enabled);
}

void KWinTabBoxConfigForm::setSwitchingModeEnabled(bool enabled)
{
    ui->switchingModeCombo->setEnabled(enabled);
}

void KWinTabBoxConfigForm::setLayoutNameEnabled(bool enabled)
{
    ui->effectCombo->setEnabled(enabled);
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
    emit filterScreenChanged(filterScreen());
}

void KWinTabBoxConfigForm::onFilterDesktop()
{
    emit filterDesktopChanged(filterDesktop());
}

void KWinTabBoxConfigForm::onFilterActivites()
{
    emit filterActivitiesChanged(filterActivities());
}

void KWinTabBoxConfigForm::onFilterMinimization()
{
    emit filterMinimizationChanged(filterMinimization());
}

void KWin::KWinTabBoxConfigForm::onApplicationMode()
{
    emit applicationModeChanged(applicationMode());
}

void KWinTabBoxConfigForm::onShowDesktopMode()
{
    emit showDesktopModeChanged(showDesktopMode());
}

void KWinTabBoxConfigForm::onSwitchingMode()
{
    emit switchingModeChanged(switchingMode());
}

void KWinTabBoxConfigForm::onEffectCombo()
{
    const bool isAddonEffect = ui->effectCombo->currentData(AddonEffect).toBool();
    ui->effectConfigButton->setIcon(QIcon::fromTheme(isAddonEffect ? "view-preview" : "configure"));
    if (!ui->kcfg_ShowTabBox->isChecked()) {
        return;
    }
    ui->kcfg_HighlightWindows->setEnabled(isAddonEffect && m_isHighlightWindowsEnabled);

    emit layoutNameChanged(layoutName());
}

void KWinTabBoxConfigForm::shortcutChanged(const QKeySequence &seq)
{
    QString action;
    if (sender()) {
        action = sender()->property("shortcutAction").toString();
    }
    if (action.isEmpty()) {
        return;
    }
    QAction *a = m_actionCollection->action(action);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << seq, KGlobalAccel::NoAutoloading);
    m_actionCollection->writeSettings();
}

} // namespace

/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "main.h"

// Qt
#include <QtDBus/QtDBus>
#include <QDesktopWidget>
#include <QVBoxLayout>

// KDE
#include <KAction>
#include <KActionCollection>
#include <KCModuleProxy>
//#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginInfo>
#include <KPluginLoader>
#include <KTabWidget>
#include <KTitleWidget>
#include <KServiceTypeTrader>
#include <KShortcutsEditor>
#include <KStandardDirs>

// own
#include "tabboxconfig.h"
#include "layoutpreview.h"

K_PLUGIN_FACTORY(KWinTabBoxConfigFactory, registerPlugin<KWin::KWinTabBoxConfig>();)
K_EXPORT_PLUGIN(KWinTabBoxConfigFactory("kcm_kwintabbox"))

namespace KWin
{

using namespace TabBox;

KWinTabBoxConfigForm::KWinTabBoxConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinTabBoxConfig::KWinTabBoxConfig(QWidget* parent, const QVariantList& args)
    : KCModule(KWinTabBoxConfigFactory::componentData(), parent, args)
    , m_config(KSharedConfig::openConfig("kwinrc"))
    , m_layoutPreview(NULL)
{
    KGlobal::locale()->insertCatalog("kwin_effects");
    KTabWidget* tabWidget = new KTabWidget(this);
    m_primaryTabBoxUi = new KWinTabBoxConfigForm(tabWidget);
    m_alternativeTabBoxUi = new KWinTabBoxConfigForm(tabWidget);
    tabWidget->addTab(m_primaryTabBoxUi, i18n("Main"));
    tabWidget->addTab(m_alternativeTabBoxUi, i18n("Alternative"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    KTitleWidget* infoLabel = new KTitleWidget(tabWidget);
    infoLabel->setText(i18n("Focus policy settings limit the functionality of navigating through windows."),
                       KTitleWidget::InfoMessage);
    infoLabel->setPixmap(KTitleWidget::InfoMessage, KTitleWidget::ImageLeft);
    layout->addWidget(infoLabel,0);
    layout->addWidget(tabWidget,1);
    setLayout(layout);

#define ADD_SHORTCUT(_NAME_, _CUT_, _BTN_) \
    a = qobject_cast<KAction*>(m_actionCollection->addAction(_NAME_));\
    a->setProperty("isConfigurationAction", true);\
    _BTN_->setProperty("shortcutAction", _NAME_);\
    a->setText(i18n(_NAME_));\
    a->setGlobalShortcut(KShortcut(_CUT_)); \
    connect(_BTN_, SIGNAL(keySequenceChanged(QKeySequence)), SLOT(shortcutChanged(QKeySequence)))

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));
    m_actionCollection->setConfigGroup("Navigation");
    m_actionCollection->setConfigGlobal(true);
    KAction* a;
    ADD_SHORTCUT("Walk Through Windows", Qt::ALT + Qt::Key_Tab, m_primaryTabBoxUi->scAll);
    ADD_SHORTCUT("Walk Through Windows (Reverse)", Qt::ALT + Qt::SHIFT + Qt::Key_Backtab,
                 m_primaryTabBoxUi->scAllReverse);
    ADD_SHORTCUT("Walk Through Windows Alternative", , m_alternativeTabBoxUi->scAll);
    ADD_SHORTCUT("Walk Through Windows Alternative (Reverse)", ,m_alternativeTabBoxUi->scAllReverse);
    ADD_SHORTCUT("Walk Through Windows of Current Application", Qt::ALT + Qt::Key_QuoteLeft,
                 m_primaryTabBoxUi->scCurrent);
    ADD_SHORTCUT("Walk Through Windows of Current Application (Reverse)", Qt::ALT + Qt::Key_AsciiTilde,
                 m_primaryTabBoxUi->scCurrentReverse);
    ADD_SHORTCUT("Walk Through Windows of Current Application Alternative", , m_alternativeTabBoxUi->scCurrent);
    ADD_SHORTCUT("Walk Through Windows of Current Application Alternative (Reverse)", ,
                 m_alternativeTabBoxUi->scCurrentReverse);
#undef ADD_SHORTCUT

    // search the effect names
    // TODO: way to recognize if a effect is not found
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    QString coverswitch;
    QString flipswitch;
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_coverswitch'");
    if (!services.isEmpty())
        coverswitch = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_flipswitch'");
    if (!services.isEmpty())
        flipswitch = services.first()->name();

    KService::List offers = trader->query("KWin/WindowSwitcher");
    QStringList layoutNames, layoutPlugins, layoutPaths;
    foreach (KService::Ptr service, offers) {
        const QString pluginName = service->property("X-KDE-PluginInfo-Name").toString();
        if (service->property("X-Plasma-API").toString() != "declarativeappletscript") {
            continue;
        }
        if (service->property("X-KWin-Exclude-Listing").toBool()) {
            continue;
        }
        const QString scriptName = service->property("X-Plasma-MainScript").toString();
        const QString scriptFile = KStandardDirs::locate("data", "kwin/tabbox/" + pluginName + "/contents/" + scriptName);
        if (scriptFile.isNull()) {
            continue;
        }

        layoutNames << service->name();
        layoutPlugins << pluginName;
        layoutPaths << scriptFile;
    }

    KWinTabBoxConfigForm *ui[2] = { m_primaryTabBoxUi, m_alternativeTabBoxUi };
    for (int i = 0; i < 2; ++i) {
        ui[i]->effectCombo->addItem(coverswitch);
        ui[i]->effectCombo->addItem(flipswitch);
        for (int j = 0; j < layoutNames.count(); ++j) {
            ui[i]->effectCombo->addItem(layoutNames[j], layoutPlugins[j]);
            ui[i]->effectCombo->setItemData(ui[i]->effectCombo->count() - 1, layoutPaths[j], Qt::UserRole+1);
        }

        ui[i]->effectConfigButton->setIcon(KIcon("view-preview"));

        connect(ui[i]->highlightWindowCheck, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->showOutlineCheck, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->showTabBox, SIGNAL(clicked(bool)), SLOT(tabBoxToggled(bool)));
        connect(ui[i]->effectCombo, SIGNAL(currentIndexChanged(int)), SLOT(changed()));
        connect(ui[i]->effectCombo, SIGNAL(currentIndexChanged(int)), SLOT(effectSelectionChanged(int)));
        connect(ui[i]->effectConfigButton, SIGNAL(clicked(bool)), SLOT(configureEffectClicked()));

        connect(ui[i]->switchingModeCombo, SIGNAL(currentIndexChanged(int)), SLOT(changed()));
        connect(ui[i]->showDesktop, SIGNAL(clicked(bool)), SLOT(changed()));

        connect(ui[i]->filterDesktops, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->currentDesktop, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->otherDesktops, SIGNAL(clicked(bool)), SLOT(changed()));

        connect(ui[i]->filterActivities, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->currentActivity, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->otherActivities, SIGNAL(clicked(bool)), SLOT(changed()));

        connect(ui[i]->filterScreens, SIGNAL(clicked(bool)), SLOT(changed()));
        if (QApplication::desktop()->screenCount() < 2) {
            ui[i]->filterScreens->hide();
            ui[i]->screenFilter->hide();
        } else {
            connect(ui[i]->currentScreen, SIGNAL(clicked(bool)), SLOT(changed()));
            connect(ui[i]->otherScreens, SIGNAL(clicked(bool)), SLOT(changed()));
        }

        connect(ui[i]->oneAppWindow, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->filterMinimization, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->visibleWindows, SIGNAL(clicked(bool)), SLOT(changed()));
        connect(ui[i]->hiddenWindows, SIGNAL(clicked(bool)), SLOT(changed()));
    }

    // check focus policy - we don't offer configs for unreasonable focus policies
    KConfigGroup config(m_config, "Windows");
    QString policy = config.readEntry("FocusPolicy", "ClickToFocus");
    if ((policy == "FocusUnderMouse") || (policy == "FocusStrictlyUnderMouse")) {
        tabWidget->setEnabled(false);
        infoLabel->show();
    } else
        infoLabel->hide();
}

KWinTabBoxConfig::~KWinTabBoxConfig()
{
}

void KWinTabBoxConfig::load()
{
    KCModule::load();

    const QString group[2] = { "TabBox", "TabBoxAlternative" };
    KWinTabBoxConfigForm* ui[2] = { m_primaryTabBoxUi, m_alternativeTabBoxUi };
    TabBoxConfig *tabBoxConfig[2] = { &m_tabBoxConfig, &m_tabBoxAlternativeConfig };

    for (int i = 0; i < 2; ++i) {
        KConfigGroup config(m_config, group[i]);
        loadConfig(config, *(tabBoxConfig[i]));

        updateUiFromConfig(ui[i], *(tabBoxConfig[i]));

        KConfigGroup effectconfig(m_config, "Plugins");
        if (effectEnabled("coverswitch", effectconfig) && KConfigGroup(m_config, "Effect-CoverSwitch").readEntry(group[i], false))
            ui[i]->effectCombo->setCurrentIndex(CoverSwitch);
        else if (effectEnabled("flipswitch", effectconfig) && KConfigGroup(m_config, "Effect-FlipSwitch").readEntry(group[i], false))
            ui[i]->effectCombo->setCurrentIndex(FlipSwitch);

        QString action;
#define LOAD_SHORTCUT(_BTN_)\
        action = ui[i]->_BTN_->property("shortcutAction").toString();\
        qDebug() << "load shortcut for " << action;\
        if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action(action)))\
            ui[i]->_BTN_->setKeySequence(a->globalShortcut().primary())
        LOAD_SHORTCUT(scAll);
        LOAD_SHORTCUT(scAllReverse);
        LOAD_SHORTCUT(scCurrent);
        LOAD_SHORTCUT(scCurrentReverse);
#undef LOAD_SHORTCUT
    }
    emit changed(false);
}

void KWinTabBoxConfig::loadConfig(const KConfigGroup& config, KWin::TabBox::TabBoxConfig& tabBoxConfig)
{
    tabBoxConfig.setClientDesktopMode(TabBoxConfig::ClientDesktopMode(
                                       config.readEntry<int>("DesktopMode", TabBoxConfig::defaultDesktopMode())));
    tabBoxConfig.setClientActivitiesMode(TabBoxConfig::ClientActivitiesMode(
                                       config.readEntry<int>("ActivitiesMode", TabBoxConfig::defaultActivitiesMode())));
    tabBoxConfig.setClientApplicationsMode(TabBoxConfig::ClientApplicationsMode(
                                       config.readEntry<int>("ApplicationsMode", TabBoxConfig::defaultApplicationsMode())));
    tabBoxConfig.setClientMinimizedMode(TabBoxConfig::ClientMinimizedMode(
                                       config.readEntry<int>("MinimizedMode", TabBoxConfig::defaultMinimizedMode())));
    tabBoxConfig.setShowDesktopMode(TabBoxConfig::ShowDesktopMode(
                                       config.readEntry<int>("ShowDesktopMode", TabBoxConfig::defaultShowDesktopMode())));
    tabBoxConfig.setClientMultiScreenMode(TabBoxConfig::ClientMultiScreenMode(
                                       config.readEntry<int>("MultiScreenMode", TabBoxConfig::defaultMultiScreenMode())));
    tabBoxConfig.setClientSwitchingMode(TabBoxConfig::ClientSwitchingMode(
                                            config.readEntry<int>("SwitchingMode", TabBoxConfig::defaultSwitchingMode())));
    tabBoxConfig.setLayout(TabBoxConfig::LayoutMode(
                               config.readEntry<int>("LayoutMode", TabBoxConfig::defaultLayoutMode())));

    tabBoxConfig.setShowOutline(config.readEntry<bool>("ShowOutline", TabBoxConfig::defaultShowOutline()));
    tabBoxConfig.setShowTabBox(config.readEntry<bool>("ShowTabBox", TabBoxConfig::defaultShowTabBox()));
    tabBoxConfig.setHighlightWindows(config.readEntry<bool>("HighlightWindows", TabBoxConfig::defaultHighlightWindow()));

    tabBoxConfig.setMinWidth(config.readEntry<int>("MinWidth", TabBoxConfig::defaultMinWidth()));
    tabBoxConfig.setMinHeight(config.readEntry<int>("MinHeight", TabBoxConfig::defaultMinHeight()));

    tabBoxConfig.setLayoutName(config.readEntry<QString>("LayoutName", TabBoxConfig::defaultLayoutName()));
    tabBoxConfig.setSelectedItemLayoutName(config.readEntry<QString>("SelectedLayoutName", TabBoxConfig::defaultSelectedItemLayoutName()));
}

void KWinTabBoxConfig::saveConfig(KConfigGroup& config, const KWin::TabBox::TabBoxConfig& tabBoxConfig)
{
    // combo boxes
    config.writeEntry("DesktopMode",        int(tabBoxConfig.clientDesktopMode()));
    config.writeEntry("ActivitiesMode",     int(tabBoxConfig.clientActivitiesMode()));
    config.writeEntry("ApplicationsMode",   int(tabBoxConfig.clientApplicationsMode()));
    config.writeEntry("MinimizedMode",      int(tabBoxConfig.clientMinimizedMode()));
    config.writeEntry("ShowDesktopMode",    int(tabBoxConfig.showDesktopMode()));
    config.writeEntry("MultiScreenMode",    int(tabBoxConfig.clientMultiScreenMode()));
    config.writeEntry("SwitchingMode",      int(tabBoxConfig.clientSwitchingMode()));
    config.writeEntry("LayoutMode",         int(tabBoxConfig.layout()));
    config.writeEntry("LayoutName",         tabBoxConfig.layoutName());
    config.writeEntry("SelectedLayoutName", tabBoxConfig.selectedItemLayoutName());

    // check boxes
    config.writeEntry("ShowOutline",      tabBoxConfig.isShowOutline());
    config.writeEntry("ShowTabBox",       tabBoxConfig.isShowTabBox());
    config.writeEntry("HighlightWindows", tabBoxConfig.isHighlightWindows());

    // spin boxes
    config.writeEntry("MinWidth",  tabBoxConfig.minWidth());
    config.writeEntry("MinHeight", tabBoxConfig.minHeight());
    config.sync();
}

void KWinTabBoxConfig::save()
{
    KCModule::save();
    KConfigGroup config(m_config, "TabBox");

    // sync ui to config
    updateConfigFromUi(m_primaryTabBoxUi, m_tabBoxConfig);
    updateConfigFromUi(m_alternativeTabBoxUi, m_tabBoxAlternativeConfig);
    saveConfig(config, m_tabBoxConfig);
    config = KConfigGroup(m_config, "TabBoxAlternative");
    saveConfig(config, m_tabBoxAlternativeConfig);

    // effects
    bool highlightWindows = m_primaryTabBoxUi->highlightWindowCheck->isChecked() ||
                            m_alternativeTabBoxUi->highlightWindowCheck->isChecked();
    const bool coverSwitch = m_primaryTabBoxUi->showTabBox->isChecked() &&
                             m_primaryTabBoxUi->effectCombo->currentIndex() == CoverSwitch;
    const bool flipSwitch = m_primaryTabBoxUi->showTabBox->isChecked() &&
                            m_primaryTabBoxUi->effectCombo->currentIndex() == FlipSwitch;
    const bool coverSwitchAlternative = m_alternativeTabBoxUi->showTabBox->isChecked() &&
                                        m_alternativeTabBoxUi->effectCombo->currentIndex() == CoverSwitch;
    const bool flipSwitchAlternative = m_alternativeTabBoxUi->showTabBox->isChecked() &&
                                       m_alternativeTabBoxUi->effectCombo->currentIndex() == FlipSwitch;

    // activate effects if not active
    KConfigGroup effectconfig(m_config, "Plugins");
    if (coverSwitch || coverSwitchAlternative)
        effectconfig.writeEntry("kwin4_effect_coverswitchEnabled", true);
    if (flipSwitch || flipSwitchAlternative)
        effectconfig.writeEntry("kwin4_effect_flipswitchEnabled", true);
    if (highlightWindows)
        effectconfig.writeEntry("kwin4_effect_highlightwindowEnabled", true);
    effectconfig.sync();
    KConfigGroup coverswitchconfig(m_config, "Effect-CoverSwitch");
    coverswitchconfig.writeEntry("TabBox", coverSwitch);
    coverswitchconfig.writeEntry("TabBoxAlternative", coverSwitchAlternative);
    coverswitchconfig.sync();
    KConfigGroup flipswitchconfig(m_config, "Effect-FlipSwitch");
    flipswitchconfig.writeEntry("TabBox", flipSwitch);
    flipswitchconfig.writeEntry("TabBoxAlternative", flipSwitchAlternative);
    flipswitchconfig.sync();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    emit changed(false);
}

void KWinTabBoxConfig::defaults()
{
    const KWinTabBoxConfigForm* ui[2] = { m_primaryTabBoxUi, m_alternativeTabBoxUi};
    for (int i = 0; i < 2; ++i) {
        // combo boxes
#define CONFIGURE(SETTING, MODE, IS, VALUE) \
    ui[i]->SETTING->setChecked(TabBoxConfig::default##MODE##Mode() IS TabBoxConfig::VALUE)
        CONFIGURE(filterDesktops, Desktop, !=, AllDesktopsClients);
        CONFIGURE(currentDesktop, Desktop, ==, OnlyCurrentDesktopClients);
        CONFIGURE(otherDesktops, Desktop, ==, ExcludeCurrentDesktopClients);
        CONFIGURE(filterActivities, Activities, !=, AllActivitiesClients);
        CONFIGURE(currentActivity, Activities, ==, OnlyCurrentActivityClients);
        CONFIGURE(otherActivities, Activities, ==, ExcludeCurrentActivityClients);
        CONFIGURE(filterScreens, MultiScreen, !=, IgnoreMultiScreen);
        CONFIGURE(currentScreen, MultiScreen, ==, OnlyCurrentScreenClients);
        CONFIGURE(otherScreens, MultiScreen, ==, ExcludeCurrentScreenClients);
//         CONFIGURE(filterApplication, Applications, !=, AllWindowsAllApplications);
//         CONFIGURE(currentApplication, Applications, ==, AllWindowsCurrentApplication);
        CONFIGURE(oneAppWindow, Applications, ==, OneWindowPerApplication);
        CONFIGURE(filterMinimization, Minimized, !=, IgnoreMinimizedStatus);
        CONFIGURE(visibleWindows, Minimized, ==, ExcludeMinimizedClients);
        CONFIGURE(hiddenWindows, Minimized, ==, OnlyMinimizedClients);

        ui[i]->switchingModeCombo->setCurrentIndex(TabBoxConfig::defaultSwitchingMode());

        // checkboxes
        ui[i]->showOutlineCheck->setChecked(TabBoxConfig::defaultShowOutline());
        ui[i]->showTabBox->setChecked(TabBoxConfig::defaultShowTabBox());
        ui[i]->highlightWindowCheck->setChecked(TabBoxConfig::defaultHighlightWindow());
        CONFIGURE(showDesktop, ShowDesktop, ==, ShowDesktopClient);
#undef CONFIGURE
        // effects
        ui[i]->effectCombo->setCurrentIndex(ui[i]->effectCombo->findData("thumbnails"));
    }

    QString action;
#define RESET_SHORTCUT(_BTN_, _CUT_) \
    action = _BTN_->property("shortcutAction").toString(); \
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action(action))) \
        a->setGlobalShortcut(KShortcut(_CUT_), KAction::ActiveShortcut, KAction::NoAutoloading)
    RESET_SHORTCUT(m_primaryTabBoxUi->scAll, Qt::ALT + Qt::Key_Tab);
    RESET_SHORTCUT(m_primaryTabBoxUi->scAllReverse, Qt::ALT + Qt::SHIFT + Qt::Key_Backtab);
    RESET_SHORTCUT(m_alternativeTabBoxUi->scAll, );
    RESET_SHORTCUT(m_alternativeTabBoxUi->scAllReverse, );
    RESET_SHORTCUT(m_primaryTabBoxUi->scCurrent, Qt::ALT + Qt::Key_QuoteLeft);
    RESET_SHORTCUT(m_primaryTabBoxUi->scCurrentReverse, Qt::ALT + Qt::Key_AsciiTilde);
    RESET_SHORTCUT(m_alternativeTabBoxUi->scCurrent, );
    RESET_SHORTCUT(m_alternativeTabBoxUi->scCurrentReverse, );
    m_actionCollection->writeSettings();
#undef RESET_SHORTCUT
    emit changed(true);
}

bool KWinTabBoxConfig::effectEnabled(const QString& effect, const KConfigGroup& cfg) const
{
    KService::List services = KServiceTypeTrader::self()->query(
                                  "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
    if (services.isEmpty())
        return false;
    QVariant v = services.first()->property("X-KDE-PluginInfo-EnabledByDefault");
    return cfg.readEntry("kwin4_effect_" + effect + "Enabled", v.toBool());
}

void KWinTabBoxConfig::updateUiFromConfig(KWinTabBoxConfigForm* ui, const KWin::TabBox::TabBoxConfig& config)
{
#define CONFIGURE(SETTING, MODE, IS, VALUE) ui->SETTING->setChecked(config.MODE##Mode() IS TabBoxConfig::VALUE)
    CONFIGURE(filterDesktops, clientDesktop, !=, AllDesktopsClients);
    CONFIGURE(currentDesktop, clientDesktop, ==, OnlyCurrentDesktopClients);
    CONFIGURE(otherDesktops, clientDesktop, ==, ExcludeCurrentDesktopClients);
    CONFIGURE(filterActivities, clientActivities, !=, AllActivitiesClients);
    CONFIGURE(currentActivity, clientActivities, ==, OnlyCurrentActivityClients);
    CONFIGURE(otherActivities, clientActivities, ==, ExcludeCurrentActivityClients);
    CONFIGURE(filterScreens, clientMultiScreen, !=, IgnoreMultiScreen);
    CONFIGURE(currentScreen, clientMultiScreen, ==, OnlyCurrentScreenClients);
    CONFIGURE(otherScreens, clientMultiScreen, ==, ExcludeCurrentScreenClients);
//     CONFIGURE(filterApplication, clientApplications, !=, AllWindowsAllApplications);
//     CONFIGURE(currentApplication, clientApplications, ==, AllWindowsCurrentApplication);
    CONFIGURE(oneAppWindow, clientApplications, ==, OneWindowPerApplication);
    CONFIGURE(filterMinimization, clientMinimized, !=, IgnoreMinimizedStatus);
    CONFIGURE(visibleWindows, clientMinimized, ==, ExcludeMinimizedClients);
    CONFIGURE(hiddenWindows, clientMinimized, ==, OnlyMinimizedClients);

    ui->switchingModeCombo->setCurrentIndex(config.clientSwitchingMode());

    // check boxes
    ui->showOutlineCheck->setChecked(config.isShowOutline());
    ui->showTabBox->setChecked(config.isShowTabBox());
    ui->highlightWindowCheck->setChecked(config.isHighlightWindows());
    ui->effectCombo->setCurrentIndex(ui->effectCombo->findData(config.layoutName()));
    CONFIGURE(showDesktop, showDesktop, ==, ShowDesktopClient);
#undef CONFIGURE
}

void KWinTabBoxConfig::updateConfigFromUi(const KWin::KWinTabBoxConfigForm* ui, TabBox::TabBoxConfig& config)
{
    if (ui->filterDesktops->isChecked())
        config.setClientDesktopMode(ui->currentDesktop->isChecked() ? TabBoxConfig::OnlyCurrentDesktopClients : TabBoxConfig::ExcludeCurrentDesktopClients);
    else
        config.setClientDesktopMode(TabBoxConfig::AllDesktopsClients);
    if (ui->filterActivities->isChecked())
        config.setClientActivitiesMode(ui->currentActivity->isChecked() ? TabBoxConfig::OnlyCurrentActivityClients : TabBoxConfig::ExcludeCurrentActivityClients);
    else
        config.setClientActivitiesMode(TabBoxConfig::AllActivitiesClients);
    if (ui->filterScreens->isChecked())
        config.setClientMultiScreenMode(ui->currentScreen->isChecked() ? TabBoxConfig::OnlyCurrentScreenClients : TabBoxConfig::ExcludeCurrentScreenClients);
    else
        config.setClientMultiScreenMode(TabBoxConfig::IgnoreMultiScreen);
//     if (ui->filterApplication->isChecked())
        config.setClientApplicationsMode(ui->oneAppWindow->isChecked() ? TabBoxConfig::OneWindowPerApplication : TabBoxConfig::AllWindowsAllApplications/*AllWindowsCurrentApplication*/);
//     else
//         config.setClientApplicationsMode(TabBoxConfig::AllWindowsAllApplications);
    if (ui->filterMinimization->isChecked())
        config.setClientMinimizedMode(ui->visibleWindows->isChecked() ? TabBoxConfig::ExcludeMinimizedClients : TabBoxConfig::OnlyMinimizedClients);
    else
        config.setClientMinimizedMode(TabBoxConfig::IgnoreMinimizedStatus);

    config.setClientSwitchingMode(TabBoxConfig::ClientSwitchingMode(ui->switchingModeCombo->currentIndex()));

    config.setShowOutline(ui->showOutlineCheck->isChecked());
    config.setShowTabBox(ui->showTabBox->isChecked());
    config.setHighlightWindows(ui->highlightWindowCheck->isChecked());
    if (ui->effectCombo->currentIndex() >= Layout) {
        config.setLayoutName(ui->effectCombo->itemData(ui->effectCombo->currentIndex()).toString());
    }
    config.setShowDesktopMode(ui->showDesktop->isChecked() ? TabBoxConfig::ShowDesktopClient : TabBoxConfig::DoNotShowDesktopClient);
}

#define CHECK_CURRENT_TABBOX_UI \
    Q_ASSERT(sender());\
    KWinTabBoxConfigForm *ui = 0;\
    QObject *dad = sender();\
    while (!ui && (dad = dad->parent()))\
        ui = qobject_cast<KWinTabBoxConfigForm*>(dad);\
    Q_ASSERT(ui);

void KWinTabBoxConfig::effectSelectionChanged(int index)
{
    CHECK_CURRENT_TABBOX_UI
    ui->effectConfigButton->setIcon(KIcon(index < Layout ? "configure" : "view-preview"));
    if (!ui->showTabBox->isChecked())
        return;
    ui->highlightWindowCheck->setEnabled(index >= Layout);
    ui->showOutlineCheck->setEnabled(index >= Layout);
    if (m_layoutPreview && m_layoutPreview->isVisible()) {
        if (index < Layout)
            m_layoutPreview->hide();
        else
            m_layoutPreview->setLayout(ui->effectCombo->itemData(index, Qt::UserRole+1).toString(), ui->effectCombo->itemText(index));
    }
}

void KWinTabBoxConfig::tabBoxToggled(bool on) {
    CHECK_CURRENT_TABBOX_UI
    on = !on || ui->effectCombo->currentIndex() >= Layout;
    ui->highlightWindowCheck->setEnabled(on);
    ui->showOutlineCheck->setEnabled(on);
    emit changed();
}

void KWinTabBoxConfig::configureEffectClicked()
{
    CHECK_CURRENT_TABBOX_UI

    const int effect = ui->effectCombo->currentIndex();
    if (effect >= Layout) {
        if (!m_layoutPreview) {
            m_layoutPreview = new LayoutPreview(this);
            m_layoutPreview->setWindowTitle(i18n("Tabbox layout preview"));
            m_layoutPreview->setWindowFlags(Qt::Dialog);
        }
        m_layoutPreview->setLayout(ui->effectCombo->itemData(effect, Qt::UserRole+1).toString(), ui->effectCombo->itemText(effect));
        m_layoutPreview->show();
    } else {
        QPointer< KDialog > configDialog = new KDialog(this);
        configDialog->setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Default);
        configDialog->setWindowTitle(ui->effectCombo->currentText());
        KCModuleProxy* proxy = new KCModuleProxy(effect == CoverSwitch ? "coverswitch_config" : "flipswitch_config");
        connect(configDialog, SIGNAL(defaultClicked()), proxy, SLOT(defaults()));

        QWidget *showWidget = new QWidget(configDialog);
        QVBoxLayout *layout = new QVBoxLayout;
        showWidget->setLayout(layout);
        layout->addWidget(proxy);
        layout->insertSpacing(-1, KDialog::marginHint());
        configDialog->setMainWidget(showWidget);

        if (configDialog->exec() == QDialog::Accepted) {
            proxy->save();
        } else {
            proxy->load();
        }
        delete configDialog;
    }
}

void KWinTabBoxConfig::shortcutChanged(const QKeySequence &seq)
{
    QString action;
    if (sender())
        action = sender()->property("shortcutAction").toString();
    if (action.isEmpty())
        return;
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action(action)))
        a->setGlobalShortcut(KShortcut(seq), KAction::ActiveShortcut, KAction::NoAutoloading);
    m_actionCollection->writeSettings();
}

} // namespace

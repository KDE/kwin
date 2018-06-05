/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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
#include <effect_builtins.h>
#include <kwin_effects_interface.h>

// Qt
#include <QtDBus>
#include <QDesktopWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QPointer>

// KDE
#include <KActionCollection>
#include <KCModuleProxy>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginInfo>
#include <KPluginLoader>
#include <KPluginTrader>
#include <KTitleWidget>
#include <KServiceTypeTrader>
#include <KShortcutsEditor>
#include <KNewStuff3/KNS3/DownloadDialog>
// Plasma
#include <KPackage/Package>
#include <KPackage/PackageLoader>

// own
#include "tabboxconfig.h"
#include "layoutpreview.h"

K_PLUGIN_FACTORY(KWinTabBoxConfigFactory, registerPlugin<KWin::KWinTabBoxConfig>();)

namespace KWin
{

using namespace TabBox;

KWinTabBoxConfigForm::KWinTabBoxConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinTabBoxConfig::KWinTabBoxConfig(QWidget* parent, const QVariantList& args)
    : KCModule(parent, args)
    , m_config(KSharedConfig::openConfig("kwinrc"))
{
    QTabWidget* tabWidget = new QTabWidget(this);
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
    a = m_actionCollection->addAction(_NAME_);\
    a->setProperty("isConfigurationAction", true);\
    _BTN_->setProperty("shortcutAction", _NAME_);\
    a->setText(i18n(_NAME_));\
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << _CUT_); \
    connect(_BTN_, SIGNAL(keySequenceChanged(QKeySequence)), SLOT(shortcutChanged(QKeySequence)))

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setConfigGroup("Navigation");
    m_actionCollection->setConfigGlobal(true);
    QAction* a;
    ADD_SHORTCUT("Walk Through Windows", Qt::ALT + Qt::Key_Tab, m_primaryTabBoxUi->scAll);
    ADD_SHORTCUT("Walk Through Windows (Reverse)", Qt::ALT + Qt::SHIFT + Qt::Key_Backtab,
                 m_primaryTabBoxUi->scAllReverse);
    ADD_SHORTCUT("Walk Through Windows Alternative", QKeySequence(), m_alternativeTabBoxUi->scAll);
    ADD_SHORTCUT("Walk Through Windows Alternative (Reverse)", QKeySequence(), m_alternativeTabBoxUi->scAllReverse);
    ADD_SHORTCUT("Walk Through Windows of Current Application", Qt::ALT + Qt::Key_QuoteLeft,
                 m_primaryTabBoxUi->scCurrent);
    ADD_SHORTCUT("Walk Through Windows of Current Application (Reverse)", Qt::ALT + Qt::Key_AsciiTilde,
                 m_primaryTabBoxUi->scCurrentReverse);
    ADD_SHORTCUT("Walk Through Windows of Current Application Alternative", QKeySequence(), m_alternativeTabBoxUi->scCurrent);
    ADD_SHORTCUT("Walk Through Windows of Current Application Alternative (Reverse)", QKeySequence(),
                 m_alternativeTabBoxUi->scCurrentReverse);
#undef ADD_SHORTCUT

    initLayoutLists();
    KWinTabBoxConfigForm *ui[2] = { m_primaryTabBoxUi, m_alternativeTabBoxUi };
    for (int i = 0; i < 2; ++i) {
        ui[i]->effectConfigButton->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
        ui[i]->ghns->setIcon(QIcon::fromTheme(QStringLiteral("get-hot-new-stuff")));

        connect(ui[i]->highlightWindowCheck, SIGNAL(clicked(bool)), SLOT(changed()));
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
        connect(ui[i]->ghns, SIGNAL(clicked(bool)), SLOT(slotGHNS()));
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


static QList<KPackage::Package> availableLnFPackages()
{
    QList<KPackage::Package> packages;
    QStringList paths;
    const QStringList dataPaths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

    for (const QString &path : dataPaths) {
        QDir dir(path + QLatin1String("/plasma/look-and-feel"));
        paths << dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    const auto &p = paths;
    for (const QString &path : p) {
        KPackage::Package pkg = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
        pkg.setPath(path);
        pkg.setFallbackPackage(KPackage::Package());
        if (!pkg.filePath("defaults").isEmpty()) {
            KSharedConfigPtr conf = KSharedConfig::openConfig(pkg.filePath("defaults"));
            KConfigGroup cg = KConfigGroup(conf, "kwinrc");
            cg = KConfigGroup(&cg, "WindowSwitcher");
            if (!cg.readEntry("LayoutName", QString()).isEmpty()) {
                packages << pkg;
            }
        }
    }

    return packages;
}

void KWinTabBoxConfig::initLayoutLists()
{
    // search the effect names
    QString coverswitch = BuiltInEffects::effectData(BuiltInEffect::CoverSwitch).displayName;
    QString flipswitch = BuiltInEffects::effectData(BuiltInEffect::FlipSwitch).displayName;

    QList<KPluginMetaData> offers = KPackage::PackageLoader::self()->listPackages("KWin/WindowSwitcher");
    QStringList layoutNames, layoutPlugins, layoutPaths;

    const auto lnfPackages = availableLnFPackages();
    for (const auto &package : lnfPackages) {
        const auto &metaData = package.metadata();
        layoutNames << metaData.name();
        layoutPlugins << metaData.pluginId();
        layoutPaths << package.filePath("windowswitcher", QStringLiteral("WindowSwitcher.qml"));
    }

    for (const auto &offer : offers) {
        const QString pluginName = offer.pluginId();
        if (offer.value("X-Plasma-API") != "declarativeappletscript") {
            continue;
        }
        //we don't have a proper servicetype
        if (offer.value("X-KWin-Exclude-Listing") == QStringLiteral("true")) {
            continue;
        }
        const QString scriptName = offer.value("X-Plasma-MainScript");
        const QString scriptFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                          QLatin1String("kwin/tabbox/") + pluginName + QLatin1String("/contents/")
                                                          + scriptName);
        if (scriptFile.isNull()) {
            continue;
        }

        layoutNames << offer.name();
        layoutPlugins << pluginName;
        layoutPaths << scriptFile;
    }

    KWinTabBoxConfigForm *ui[2] = { m_primaryTabBoxUi, m_alternativeTabBoxUi };
    for (int i=0; i<2; ++i) {
        int index = ui[i]->effectCombo->currentIndex();
        QVariant data = ui[i]->effectCombo->itemData(index);
        ui[i]->effectCombo->clear();
        ui[i]->effectCombo->addItem(coverswitch);
        ui[i]->effectCombo->addItem(flipswitch);
        for (int j = 0; j < layoutNames.count(); ++j) {
            ui[i]->effectCombo->addItem(layoutNames[j], layoutPlugins[j]);
            ui[i]->effectCombo->setItemData(ui[i]->effectCombo->count() - 1, layoutPaths[j], Qt::UserRole+1);
        }
        if (data.isValid()) {
            ui[i]->effectCombo->setCurrentIndex(ui[i]->effectCombo->findData(data));
        } else if (index != -1) {
            ui[i]->effectCombo->setCurrentIndex(index);
        }
    }
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
        if (effectEnabled(BuiltInEffect::CoverSwitch, effectconfig) && KConfigGroup(m_config, "Effect-CoverSwitch").readEntry(group[i], false))
            ui[i]->effectCombo->setCurrentIndex(CoverSwitch);
        else if (effectEnabled(BuiltInEffect::FlipSwitch, effectconfig) && KConfigGroup(m_config, "Effect-FlipSwitch").readEntry(group[i], false))
            ui[i]->effectCombo->setCurrentIndex(FlipSwitch);

        QString action;
#define LOAD_SHORTCUT(_BTN_)\
        action = ui[i]->_BTN_->property("shortcutAction").toString();\
        qDebug() << "load shortcut for " << action;\
        if (QAction *a = m_actionCollection->action(action)) { \
            auto shortcuts = KGlobalAccel::self()->shortcut(a); \
            if (!shortcuts.isEmpty()) \
                ui[i]->_BTN_->setKeySequence(shortcuts.first()); \
        }
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

    tabBoxConfig.setShowTabBox(config.readEntry<bool>("ShowTabBox", TabBoxConfig::defaultShowTabBox()));
    tabBoxConfig.setHighlightWindows(config.readEntry<bool>("HighlightWindows", TabBoxConfig::defaultHighlightWindow()));

    tabBoxConfig.setLayoutName(config.readEntry<QString>("LayoutName", TabBoxConfig::defaultLayoutName()));
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
    config.writeEntry("LayoutName",         tabBoxConfig.layoutName());

    // check boxes
    config.writeEntry("ShowTabBox",       tabBoxConfig.isShowTabBox());
    config.writeEntry("HighlightWindows", tabBoxConfig.isHighlightWindows());

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
        effectconfig.writeEntry("coverswitchEnabled", true);
    if (flipSwitch || flipSwitchAlternative)
        effectconfig.writeEntry("flipswitchEnabled", true);
    if (highlightWindows)
        effectconfig.writeEntry("highlightwindowEnabled", true);
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
    // and reconfigure the effects
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                             QStringLiteral("/Effects"),
                                             QDBusConnection::sessionBus());
    interface.reconfigureEffect(BuiltInEffects::nameForEffect(BuiltInEffect::CoverSwitch));
    interface.reconfigureEffect(BuiltInEffects::nameForEffect(BuiltInEffect::FlipSwitch));

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
        CONFIGURE(oneAppWindow, Applications, ==, OneWindowPerApplication);
        CONFIGURE(filterMinimization, Minimized, !=, IgnoreMinimizedStatus);
        CONFIGURE(visibleWindows, Minimized, ==, ExcludeMinimizedClients);
        CONFIGURE(hiddenWindows, Minimized, ==, OnlyMinimizedClients);

        ui[i]->switchingModeCombo->setCurrentIndex(TabBoxConfig::defaultSwitchingMode());

        // checkboxes
        ui[i]->showTabBox->setChecked(TabBoxConfig::defaultShowTabBox());
        ui[i]->highlightWindowCheck->setChecked(TabBoxConfig::defaultHighlightWindow());
        CONFIGURE(showDesktop, ShowDesktop, ==, ShowDesktopClient);
#undef CONFIGURE
        // effects
        ui[i]->effectCombo->setCurrentIndex(ui[i]->effectCombo->findData("sidebar"));
    }

    QString action;
    auto RESET_SHORTCUT = [this](KKeySequenceWidget *widget, const QKeySequence &sequence = QKeySequence()) {
        const QString action = widget->property("shortcutAction").toString();
        QAction *a = m_actionCollection->action(action);
        KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << sequence, KGlobalAccel::NoAutoloading);
    };
    RESET_SHORTCUT(m_primaryTabBoxUi->scAll, Qt::ALT + Qt::Key_Tab);
    RESET_SHORTCUT(m_primaryTabBoxUi->scAllReverse, Qt::ALT + Qt::SHIFT + Qt::Key_Backtab);
    RESET_SHORTCUT(m_alternativeTabBoxUi->scAll);
    RESET_SHORTCUT(m_alternativeTabBoxUi->scAllReverse);
    RESET_SHORTCUT(m_primaryTabBoxUi->scCurrent, Qt::ALT + Qt::Key_QuoteLeft);
    RESET_SHORTCUT(m_primaryTabBoxUi->scCurrentReverse, Qt::ALT + Qt::Key_AsciiTilde);
    RESET_SHORTCUT(m_alternativeTabBoxUi->scCurrent);
    RESET_SHORTCUT(m_alternativeTabBoxUi->scCurrentReverse);
    m_actionCollection->writeSettings();
    emit changed(true);
}

bool KWinTabBoxConfig::effectEnabled(const BuiltInEffect& effect, const KConfigGroup& cfg) const
{
    return cfg.readEntry(BuiltInEffects::nameForEffect(effect) + "Enabled", BuiltInEffects::enabledByDefault(effect));
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
    CONFIGURE(oneAppWindow, clientApplications, ==, OneWindowPerApplication);
    CONFIGURE(filterMinimization, clientMinimized, !=, IgnoreMinimizedStatus);
    CONFIGURE(visibleWindows, clientMinimized, ==, ExcludeMinimizedClients);
    CONFIGURE(hiddenWindows, clientMinimized, ==, OnlyMinimizedClients);

    ui->switchingModeCombo->setCurrentIndex(config.clientSwitchingMode());

    // check boxes
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
    config.setClientApplicationsMode(ui->oneAppWindow->isChecked() ? TabBoxConfig::OneWindowPerApplication : TabBoxConfig::AllWindowsAllApplications);
    if (ui->filterMinimization->isChecked())
        config.setClientMinimizedMode(ui->visibleWindows->isChecked() ? TabBoxConfig::ExcludeMinimizedClients : TabBoxConfig::OnlyMinimizedClients);
    else
        config.setClientMinimizedMode(TabBoxConfig::IgnoreMinimizedStatus);

    config.setClientSwitchingMode(TabBoxConfig::ClientSwitchingMode(ui->switchingModeCombo->currentIndex()));

    config.setShowTabBox(ui->showTabBox->isChecked());
    config.setHighlightWindows(ui->highlightWindowCheck->isChecked());
    if (ui->effectCombo->currentIndex() >= Layout) {
        config.setLayoutName(ui->effectCombo->itemData(ui->effectCombo->currentIndex()).toString());
    }
    config.setShowDesktopMode(ui->showDesktop->isChecked() ? TabBoxConfig::ShowDesktopClient : TabBoxConfig::DoNotShowDesktopClient);
}

#define CHECK_CURRENT_TABBOX_UI \
    Q_ASSERT(sender());\
    KWinTabBoxConfigForm *ui = nullptr;\
    QObject *dad = sender();\
    while (!ui && (dad = dad->parent()))\
        ui = qobject_cast<KWinTabBoxConfigForm*>(dad);\
    Q_ASSERT(ui);

void KWinTabBoxConfig::effectSelectionChanged(int index)
{
    CHECK_CURRENT_TABBOX_UI
    ui->effectConfigButton->setIcon(QIcon::fromTheme(index < Layout ? "configure" : "view-preview"));
    if (!ui->showTabBox->isChecked())
        return;
    ui->highlightWindowCheck->setEnabled(index >= Layout);
}

void KWinTabBoxConfig::tabBoxToggled(bool on) {
    CHECK_CURRENT_TABBOX_UI
    on = !on || ui->effectCombo->currentIndex() >= Layout;
    ui->highlightWindowCheck->setEnabled(on);
    emit changed();
}

void KWinTabBoxConfig::configureEffectClicked()
{
    CHECK_CURRENT_TABBOX_UI

    const int effect = ui->effectCombo->currentIndex();
    if (effect >= Layout) {
        // TODO: here we need to show the preview
        new LayoutPreview(ui->effectCombo->itemData(effect, Qt::UserRole+1).toString(), this);
    } else {
        QPointer<QDialog> configDialog = new QDialog(this);
        configDialog->setLayout(new QVBoxLayout);
        configDialog->setWindowTitle(ui->effectCombo->currentText());
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults, configDialog);
        connect(buttonBox, SIGNAL(accepted()), configDialog, SLOT(accept()));
        connect(buttonBox, SIGNAL(rejected()), configDialog, SLOT(reject()));

        const QString name = BuiltInEffects::nameForEffect(effect == CoverSwitch ? BuiltInEffect::CoverSwitch : BuiltInEffect::FlipSwitch);

        KCModule *kcm = KPluginTrader::createInstanceFromQuery<KCModule>(QStringLiteral("kwin/effects/configs/"), QString(),
                                                                         QStringLiteral("'%1' in [X-KDE-ParentComponents]").arg(name),
                                                                        configDialog);
        if (!kcm) {
            delete configDialog;
            return;
        }

        connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, kcm, &KCModule::defaults);

        QWidget *showWidget = new QWidget(configDialog);
        QVBoxLayout *layout = new QVBoxLayout;
        showWidget->setLayout(layout);
        layout->addWidget(kcm);
        configDialog->layout()->addWidget(showWidget);
        configDialog->layout()->addWidget(buttonBox);

        if (configDialog->exec() == QDialog::Accepted) {
            kcm->save();
        } else {
            kcm->load();
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
    QAction *a = m_actionCollection->action(action);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << seq, KGlobalAccel::NoAutoloading);
    m_actionCollection->writeSettings();
}

void KWinTabBoxConfig::slotGHNS()
{
    QPointer<KNS3::DownloadDialog> downloadDialog = new KNS3::DownloadDialog("kwinswitcher.knsrc", this);
    if (downloadDialog->exec() == QDialog::Accepted) {
        if (!downloadDialog->changedEntries().isEmpty()) {
            initLayoutLists();
        }
    }
    delete downloadDialog;
}

} // namespace

#include "main.moc"

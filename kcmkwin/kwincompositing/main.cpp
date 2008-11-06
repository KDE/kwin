/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "advanced.h"
#include "kwin_interface.h"

#include <kaboutdata.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <ksettings/dispatcher.h>
#include <kpluginselector.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <kservice.h>
#include <ktitlewidget.h>

#include <QtDBus/QtDBus>
#include <QTimer>
#include <QLabel>
#include <KPluginFactory>
#include <KPluginLoader>

K_PLUGIN_FACTORY(KWinCompositingConfigFactory,
        registerPlugin<KWin::KWinCompositingConfig>();
        )
K_EXPORT_PLUGIN(KWinCompositingConfigFactory("kcmkwincompositing"))

namespace KWin
{


ConfirmDialog::ConfirmDialog() :
        KTimerDialog(10000, KTimerDialog::CountDown, 0,
                     i18n("Confirm Desktop Effects Change"), KTimerDialog::Ok|KTimerDialog::Cancel,
                     KTimerDialog::Cancel)
    {
    setObjectName( "mainKTimerDialog" );
    setButtonGuiItem( KDialog::Ok, KGuiItem( i18n( "&Accept Configuration" ), "dialog-ok" ));
    setButtonGuiItem( KDialog::Cancel, KGuiItem( i18n( "&Return to Previous Configuration" ), "dialog-cancel" ));

    QLabel *label = new QLabel( i18n( "Desktop effects settings have changed.\n"
            "Do you want to keep the new settings?\n"
            "They will be automatically reverted in 10 seconds." ), this );
    label->setWordWrap( true );
    setMainWidget( label );
    }


KWinCompositingConfig::KWinCompositingConfig(QWidget *parent, const QVariantList &)
    : KCModule( KWinCompositingConfigFactory::componentData(), parent),
        mKWinConfig(KSharedConfig::openConfig("kwinrc")),
        m_showConfirmDialog (false)
{
    KGlobal::locale()->insertCatalog( "kwin_effects" );
    ui.setupUi(this);
    layout()->setMargin(0);
    ui.tabWidget->setCurrentIndex(0);
    ui.statusTitleWidget->hide();
    setupElectricBorders();

    connect(ui.advancedOptions, SIGNAL(clicked()), this, SLOT(showAdvancedOptions()));
    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(compositingEnabled(bool)));
    connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));

    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectWinManagement, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectShadows, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectAnimations, SIGNAL(toggled(bool)), this, SLOT(changed()));

    connect(ui.effectSelector, SIGNAL(changed(bool)), this, SLOT(changed()));
    connect(ui.effectSelector, SIGNAL(configCommitted(const QByteArray&)),
            this, SLOT(reparseConfiguration(const QByteArray&)));

    connect(ui.windowSwitchingCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.desktopSwitchingCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.animationSpeedCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    connect(ui.edges_monitor, SIGNAL(changed()), this, SLOT(changed()));
    connect(ui.edges_monitor, SIGNAL(edgeSelectionChanged(int,int)), this, SLOT(electricBorderSelectionChanged(int,int)));

    // Open the temporary config file
    // Temporary conf file is used to synchronize effect checkboxes with effect
    //  selector by loading/saving effects from/to temp config when active tab
    //  changes.
    mTmpConfigFile.open();
    mTmpConfig = KSharedConfig::openConfig(mTmpConfigFile.fileName());

    if( CompositingPrefs::compositingPossible() )
    {
        // Driver-specific config detection
        mDefaultPrefs.detect();

        initEffectSelector();

        // Load config
        load();
    }
    else
    {
        ui.useCompositing->setEnabled(false);
        ui.useCompositing->setChecked(false);
        compositingEnabled(false);

        QString text = i18n("Compositing is not supported on your system.");
        text += "<br>";
        text += CompositingPrefs::compositingNotPossibleReason();
        ui.statusTitleWidget->setText(text);
        ui.statusTitleWidget->setPixmap(KTitleWidget::InfoMessage, KTitleWidget::ImageLeft);
        ui.statusTitleWidget->show();
    }

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwincompositing"), 0,
            ki18n("KWin Desktop Effects Configuration Module"),
            0, KLocalizedString(), KAboutData::License_GPL, ki18n("(c) 2007 Rivo Laks"));
    about->addAuthor(ki18n("Rivo Laks"), KLocalizedString(), "rivolaks@hot.ee");
    setAboutData(about);

    // search the effect names
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    QString boxswitch, presentwindows, coverswitch, flipswitch, slide, cube;
    // window switcher
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_boxswitch'");
    if( !services.isEmpty() )
        boxswitch = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_presentwindows'");
    if( !services.isEmpty() )
        presentwindows = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_coverswitch'");
    if( !services.isEmpty() )
        coverswitch = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_flipswitch'");
    if( !services.isEmpty() )
        flipswitch = services.first()->name();
    // desktop switcher
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_slide'");
    if( !services.isEmpty() )
        slide = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_cube'");
    if( !services.isEmpty() )
        cube = services.first()->name();
    // init the combo boxes
    ui.windowSwitchingCombo->addItem(i18n("No Effect"));
    ui.windowSwitchingCombo->addItem(boxswitch);
    ui.windowSwitchingCombo->addItem(presentwindows);
    ui.windowSwitchingCombo->addItem(coverswitch);
    ui.windowSwitchingCombo->addItem(flipswitch);

    ui.desktopSwitchingCombo->addItem(i18n("No Effect"));
    ui.desktopSwitchingCombo->addItem(slide);
    ui.desktopSwitchingCombo->addItem(cube);
}

KWinCompositingConfig::~KWinCompositingConfig()
{
}

void KWinCompositingConfig::reparseConfiguration(const QByteArray&conf)
{
    KSettings::Dispatcher::reparseConfiguration(conf);
}

void KWinCompositingConfig::compositingEnabled(bool enabled)
{
    ui.compositingOptionsContainer->setEnabled(enabled);
    ui.tabWidget->setTabEnabled(1, enabled);
}

void KWinCompositingConfig::showAdvancedOptions()
{
    KWinAdvancedCompositingOptions* dialog = new KWinAdvancedCompositingOptions(this, mKWinConfig, &mDefaultPrefs);

    dialog->show();
}

void KWinCompositingConfig::showConfirmDialog()
{
    bool revert = false;
    // Feel free to extend this to support several kwin instances (multihead) if you
    // think it makes sense.
    OrgKdeKWinInterface kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
    if( !kwin.waitForCompositingSetup().value())
    {
        KMessageBox::sorry( this, i18n(
            "Failed to activate desktop effects using the given "
            "configuration options. Settings will be reverted to their previous values.\n\n"
            "Check your X configuration. You may also consider changing advanced options, "
            "especially changing the compositing type." ));
        revert = true;
    }
    else
    {
        ConfirmDialog confirm;
        if( !confirm.exec())
            revert = true;
    }
    if( revert )
    {
        // Revert settings
        KConfigGroup config(mKWinConfig, "Compositing");
        config.deleteGroup();
        QMap<QString, QString>::const_iterator it = mPreviousConfig.constBegin();
        for(; it != mPreviousConfig.constEnd(); ++it) {
            if (it.value().isEmpty())
                continue;
            config.writeEntry(it.key(), it.value());
        }
        // Sync with KWin and reload
        configChanged();
        load();
    }
}

void KWinCompositingConfig::initEffectSelector()
{
    // Find all .desktop files of the effects
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QList<KPluginInfo> effectinfos = KPluginInfo::fromServices(offers);

    // Add them to the plugin selector
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Appearance"), "Appearance", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Accessibility"), "Accessibility", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Focus"), "Focus", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Window Management"), "Window Management", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Candy"), "Candy", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Demos"), "Demos", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tests"), "Tests", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tools"), "Tools", mTmpConfig);
}

void KWinCompositingConfig::currentTabChanged(int tab)
{
    // block signals to don't emit the changed()-signal by just switching the current tab
    blockSignals(true);
    // write possible changes done to synchronize effect checkboxes and selector
    if (tab == 0)
    {
        // General tab was activated
        saveEffectsTab();
        loadGeneralTab();
    }
    else
    {
        // Effects tab was activated
        saveGeneralTab();
        loadEffectsTab();
    }
    blockSignals(false);
}

void KWinCompositingConfig::loadGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    ui.useCompositing->setChecked(config.readEntry("Enabled", mDefaultPrefs.enableCompositing()));
    ui.animationSpeedCombo->setCurrentIndex(config.readEntry("AnimationSpeed", 3 ));

    // Load effect settings
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define LOAD_EFFECT_CONFIG(effectname)  effectconfig.readEntry("kwin4_effect_" effectname "Enabled", true)
    int winManagementEnabled = LOAD_EFFECT_CONFIG("presentwindows")
        + LOAD_EFFECT_CONFIG("desktopgrid")
        + LOAD_EFFECT_CONFIG("dialogparent");
    if (winManagementEnabled > 0 && winManagementEnabled < 3) {
        ui.effectWinManagement->setTristate(true);
        ui.effectWinManagement->setCheckState(Qt::PartiallyChecked);
    }
    else
        ui.effectWinManagement->setChecked(winManagementEnabled);
    ui.effectShadows->setChecked(LOAD_EFFECT_CONFIG("shadow"));
    ui.effectAnimations->setChecked(LOAD_EFFECT_CONFIG("minimizeanimation"));
#undef LOAD_EFFECT_CONFIG

    // window switching
    if( effectEnabled( "boxswitch", effectconfig ))
        ui.windowSwitchingCombo->setCurrentIndex( 1 );
    if( effectEnabled( "coverswitch", effectconfig ))
        ui.windowSwitchingCombo->setCurrentIndex( 3 );
    if( effectEnabled( "flipswitch", effectconfig ))
        ui.windowSwitchingCombo->setCurrentIndex( 4 );
    KConfigGroup presentwindowsconfig(mKWinConfig, "Effect-PresentWindows");
    if( effectEnabled( "presentwindows", effectconfig ) && presentwindowsconfig.readEntry("TabBox", false) )
        ui.windowSwitchingCombo->setCurrentIndex( 2 );

    // desktop switching
    if( effectEnabled( "slide", effectconfig ))
        ui.desktopSwitchingCombo->setCurrentIndex( 1 );
    KConfigGroup cubeconfig(mKWinConfig, "Effect-Cube");
    if( effectEnabled( "cube", effectconfig ) && cubeconfig.readEntry("AnimateDesktopChange", false))
        ui.desktopSwitchingCombo->setCurrentIndex( 2 );

    loadElectricBorders();
}

bool KWinCompositingConfig::effectEnabled( const QString& effect, const KConfigGroup& cfg ) const
{
    KService::List services = KServiceTypeTrader::self()->query(
        "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + "'");
    if( services.isEmpty())
        return false;
    QVariant v = services.first()->property("X-KDE-PluginInfo-EnabledByDefault");
    return cfg.readEntry("kwin4_effect_" + effect + "Enabled", v.toBool());
}

void KWinCompositingConfig::loadEffectsTab()
{
    ui.effectSelector->load();
}

void KWinCompositingConfig::load()
{
    mKWinConfig->reparseConfiguration();

    // Copy Plugins group to temp config file
    QMap<QString, QString> entries = mKWinConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup tmpconfig(mTmpConfig, "Plugins");
    tmpconfig.deleteGroup();
    for(; it != entries.constEnd(); ++it)
    {
        tmpconfig.writeEntry(it.key(), it.value());
    }

    loadGeneralTab();
    loadEffectsTab();

    emit changed( false );
}

void KWinCompositingConfig::saveGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    // Save current config. We'll use this for restoring in case something
    //  goes wrong.
    mPreviousConfig = config.entryMap();
    // Check if any critical settings that need confirmation have changed
    if(ui.useCompositing->isChecked()
        && ui.useCompositing->isChecked() != config.readEntry("Enabled", mDefaultPrefs.enableCompositing()))
        {
        m_showConfirmDialog = true;
        }

    config.writeEntry("Enabled", ui.useCompositing->isChecked());
    config.writeEntry("AnimationSpeed", ui.animationSpeedCombo->currentIndex());

    // Save effects
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define WRITE_EFFECT_CONFIG(effectname, widget)  effectconfig.writeEntry("kwin4_effect_" effectname "Enabled", widget->isChecked())
    if (ui.effectWinManagement->checkState() != Qt::PartiallyChecked) {
        WRITE_EFFECT_CONFIG("presentwindows", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("desktopgrid", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("dialogparent", ui.effectWinManagement);
    }
    WRITE_EFFECT_CONFIG("shadow", ui.effectShadows);
    // TODO: maybe also do some effect-specific configuration here, e.g.
    //  enable/disable desktopgrid's animation according to this setting
    WRITE_EFFECT_CONFIG("minimizeanimation", ui.effectAnimations);
#undef WRITE_EFFECT_CONFIG

    int windowSwitcher = ui.windowSwitchingCombo->currentIndex();
    bool presentWindowSwitching = false;
    switch( windowSwitcher )
        {
        case 0:
            // no effect
            effectconfig.writeEntry("kwin4_effect_boxswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_coverswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_flipswitchEnabled", false);
            break;
        case 1:
            // box switch
            effectconfig.writeEntry("kwin4_effect_boxswitchEnabled", true);
            effectconfig.writeEntry("kwin4_effect_coverswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_flipswitchEnabled", false);
            break;
        case 2:
            // present windows
            presentWindowSwitching = true;
            effectconfig.writeEntry("kwin4_effect_presentwindowsEnabled", true);
            effectconfig.writeEntry("kwin4_effect_boxswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_coverswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_flipswitchEnabled", false);
            break;
        case 3:
            // coverswitch
            effectconfig.writeEntry("kwin4_effect_boxswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_coverswitchEnabled", true);
            effectconfig.writeEntry("kwin4_effect_flipswitchEnabled", false);
            break;
        case 4:
            // flipswitch
            effectconfig.writeEntry("kwin4_effect_boxswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_coverswitchEnabled", false);
            effectconfig.writeEntry("kwin4_effect_flipswitchEnabled", true);
            break;
        }
    KConfigGroup presentwindowsconfig(mKWinConfig, "Effect-PresentWindows");
    presentwindowsconfig.writeEntry("TabBox", presentWindowSwitching);

    int desktopSwitcher = ui.desktopSwitchingCombo->currentIndex();
    bool cubeDesktopSwitching = false;
    switch( desktopSwitcher )
        {
        case 0:
            // no effect
            effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
            break;
        case 1:
            // slide
            effectconfig.writeEntry("kwin4_effect_slideEnabled", true);
            break;
        case 2:
            // cube
            cubeDesktopSwitching = true;
            effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
            effectconfig.writeEntry("kwin4_effect_cubeEnabled", true);
            break;
        }
    KConfigGroup cubeconfig(mKWinConfig, "Effect-Cube");
    cubeconfig.writeEntry("AnimateDesktopChange", cubeDesktopSwitching);

    saveElectricBorders();
}

void KWinCompositingConfig::saveEffectsTab()
{
    ui.effectSelector->save();
}

void KWinCompositingConfig::save()
{
    // bah; tab content being dependent on the other is really bad; and
    // deprecated in the HIG for a reason.  Its confusing!
    // Make sure we only call save on each tab once; as they are stateful due to the revert concept
    if (ui.tabWidget->currentIndex() == 0) { // general was active
        saveGeneralTab();
        loadEffectsTab();
        saveEffectsTab();
    }
    else { // effects tab was active
        saveEffectsTab();
        loadGeneralTab();
        saveGeneralTab();
    }

    // Copy Plugins group from temp config to real config
    QMap<QString, QString> entries = mTmpConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup realconfig(mKWinConfig, "Plugins");
    realconfig.deleteGroup();
    for(; it != entries.constEnd(); ++it)
    {
        realconfig.writeEntry(it.key(), it.value());
    }

    emit changed( false );

    configChanged();

    if(m_showConfirmDialog)
    {
        m_showConfirmDialog = false;
        showConfirmDialog();
    }
}

void KWinCompositingConfig::configChanged()
{
    // Send signal to kwin
    mKWinConfig->sync();
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}


void KWinCompositingConfig::defaults()
{
    ui.tabWidget->setCurrentIndex(0);

    ui.useCompositing->setChecked(mDefaultPrefs.enableCompositing());
    ui.effectWinManagement->setChecked(true);
    ui.effectShadows->setChecked(true);
    ui.effectAnimations->setChecked(true);

    ui.windowSwitchingCombo->setCurrentIndex( 1 );
    ui.desktopSwitchingCombo->setCurrentIndex( 1 );
    ui.animationSpeedCombo->setCurrentIndex( 3 );

    ui.effectSelector->defaults();

    for( int i=0; i<8; i++ )
        {
        // set all edges to no effect
        ui.edges_monitor->selectEdgeItem( i, 0 );
        }
    // set top left to present windows
    ui.edges_monitor->selectEdgeItem( (int)Monitor::TopLeft, (int)PresentWindowsAll );
}

QString KWinCompositingConfig::quickHelp() const
{
    return i18n("<h1>Desktop Effects</h1>");
}


void KWinCompositingConfig::setupElectricBorders()
{
    addItemToEdgesMonitor( i18n("No Effect"));

    // search the effect names
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_presentwindows'");
    if( !services.isEmpty() )
        {
        addItemToEdgesMonitor( services.first()->name() + " - " + i18n( "All desktops" ));
        addItemToEdgesMonitor( services.first()->name() + " - " + i18n( "Current desktop" ));
        }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_desktopgrid'");
    if( !services.isEmpty() )
        {
        addItemToEdgesMonitor( services.first()->name());
        }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_cube'");
    if( !services.isEmpty() )
        {
        addItemToEdgesMonitor( services.first()->name());
        }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_cylinder'");
    if( !services.isEmpty() )
        {
        addItemToEdgesMonitor( services.first()->name());
        }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_sphere'");
    if( !services.isEmpty() )
        {
        addItemToEdgesMonitor( services.first()->name());
        }
}

void KWinCompositingConfig::addItemToEdgesMonitor(const QString& item)
{
    for( int i=0; i<8; i++ )
        {
        ui.edges_monitor->addEdgeItem( i, item );
        }
}

void KWinCompositingConfig::electricBorderSelectionChanged(int edge, int index)
{
    if( index == (int)NoEffect )
        return;
    for( int i=0; i<8; i++)
        {
        if( i == edge )
            continue;
        if( ui.edges_monitor->selectedEdgeItem( i ) == index )
            ui.edges_monitor->selectEdgeItem( i, (int)NoEffect );
        }
}


void KWinCompositingConfig::loadElectricBorders()
{
    // Present Windows
    KConfigGroup presentwindowsconfig(mKWinConfig, "Effect-PresentWindows");
    changeElectricBorder( (ElectricBorder)presentwindowsconfig.readEntry( "BorderActivateAll",
        int( ElectricTopLeft )), (int)PresentWindowsAll );
    changeElectricBorder( (ElectricBorder)presentwindowsconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)PresentWindowsCurrent );
    // Desktop Grid
    KConfigGroup gridconfig(mKWinConfig, "Effect-DesktopGrid");
    changeElectricBorder( (ElectricBorder)gridconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)DesktopGrid );
    // Desktop Cube
    KConfigGroup cubeconfig(mKWinConfig, "Effect-Cube");
    changeElectricBorder( (ElectricBorder)cubeconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)Cube );
    // Desktop Cylinder
    KConfigGroup cylinderconfig(mKWinConfig, "Effect-Cylinder");
    changeElectricBorder( (ElectricBorder)cylinderconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)Cylinder );
    // Desktop Grid
    KConfigGroup sphereconfig(mKWinConfig, "Effect-Sphere");
    changeElectricBorder( (ElectricBorder)sphereconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)Sphere );
}

void KWinCompositingConfig::changeElectricBorder( ElectricBorder border, int index )
{
    switch (border)
        {
        case ElectricTop:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::Top, index );
            break;
        case ElectricTopRight:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::TopRight, index );
            break;
        case ElectricRight:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::Right, index );
            break;
        case ElectricBottomRight:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::BottomRight, index );
            break;
        case ElectricBottom:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::Bottom, index );
            break;
        case ElectricBottomLeft:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::BottomLeft, index );
            break;
        case ElectricLeft:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::Left, index );
            break;
        case ElectricTopLeft:
            ui.edges_monitor->selectEdgeItem( (int)Monitor::TopLeft, index );
            break;
        default:
            // nothing
            break;
        }
}

ElectricBorder KWinCompositingConfig::checkEffectHasElectricBorder( int index )
{
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::Top ) == index )
        {
        return ElectricTop;
        }
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::TopRight ) == index )
        {
        return ElectricTopRight;
        }
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::Right ) == index )
        {
        return ElectricRight;
        }
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::BottomRight ) == index )
        {
        return ElectricBottomRight;
        }
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::Bottom ) == index )
        {
        return ElectricBottom;
        }
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::BottomLeft ) == index )
        {
        return ElectricBottomLeft;
        }
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::Left ) == index )
        {
        return ElectricLeft;
        }
    if( ui.edges_monitor->selectedEdgeItem( (int)Monitor::TopLeft ) == index )
        {
        return ElectricTopLeft;
        }
    return ElectricNone;
}

void KWinCompositingConfig::saveElectricBorders()
{
    KConfigGroup presentwindowsconfig(mKWinConfig, "Effect-PresentWindows");
    presentwindowsconfig.writeEntry( "BorderActivateAll", (int)checkEffectHasElectricBorder( (int)PresentWindowsAll ));
    presentwindowsconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)PresentWindowsCurrent ));

    KConfigGroup gridconfig(mKWinConfig, "Effect-DesktopGrid");
    gridconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)DesktopGrid ));

    KConfigGroup cubeconfig(mKWinConfig, "Effect-Cube");
    cubeconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)Cube ));

    KConfigGroup cylinderconfig(mKWinConfig, "Effect-Cylinder");
    cylinderconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)Cylinder ));

    KConfigGroup sphereconfig(mKWinConfig, "Effect-Sphere");
    sphereconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)Sphere ));
}
    
} // namespace
    
#include "main.moc"
        
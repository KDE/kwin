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

#include "kwin_interface.h"

#include <kaboutdata.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <ksettings/dispatcher.h>
#include <kpluginselector.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <kservice.h>
#include <ktitlewidget.h>

#include <QFile>
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
        mNewConfig( KSharedConfig::openConfig( "kwinrc" ) )
    {
    KGlobal::locale()->insertCatalog( "kwin_effects" );
    ui.setupUi(this);
    layout()->setMargin(0);
    ui.tabWidget->setCurrentIndex(0);
    ui.statusTitleWidget->hide();
    setupElectricBorders();

    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(compositingEnabled(bool)));
    connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));

    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectWinManagement, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectShadows, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectAnimations, SIGNAL(toggled(bool)), this, SLOT(changed()));

    connect(ui.effectSelector, SIGNAL(changed(bool)), this, SLOT(changed()));

    connect(ui.windowSwitchingCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.desktopSwitchingCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.animationSpeedCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    connect(ui.edges_monitor, SIGNAL(changed()), this, SLOT(changed()));
    connect(ui.edges_monitor, SIGNAL(edgeSelectionChanged(int,int)), this, SLOT(electricBorderSelectionChanged(int,int)));

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.windowThumbnails, SIGNAL(activated(int)), this, SLOT(changed()));
    connect(ui.disableChecks, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glTextureFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glDirect, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glVSync, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.xrenderSmoothScale, SIGNAL(toggled(bool)), this, SLOT(changed()));

    // TODO: Apparently nobody can tell why this slot should be needed. Could possibly be removed.
    connect( ui.effectSelector, SIGNAL( configCommitted( const QByteArray& ) ),
            this, SLOT( reparseConfiguration( const QByteArray& ) ) );

    // Create the backup config. It will be used as a means to revert the config if
    // the X server crashes and to determine if a confirmation dialog should be shown.
    // After the new settings have been confirmed to be stable, it is updated with the new
    // config. It will be written to disk before a new config is loaded and is deleted after
    // the user has confirmed the new settings.
    mBackupConfig = new KConfig( mNewConfig->name() + '~', KConfig::SimpleConfig );
    updateBackupWithNewConfig();
    // If the backup file already exists, it is a residue we want to clean up; it would
    // probably be in an invalid state anyway.
    deleteBackupConfigFile();

    if( CompositingPrefs::compositingPossible() )
        {
        // Driver-specific config detection
        mDefaultPrefs.detect();
        initEffectSelector();

        // Initialize the user interface with the config loaded from kwinrc.
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
    // Make sure that the backup config is not written to disk.
    mBackupConfig->markAsClean();
    delete mBackupConfig;
    }

void KWinCompositingConfig::reparseConfiguration( const QByteArray& conf )
    {
    KSettings::Dispatcher::reparseConfiguration( conf );
    }

void KWinCompositingConfig::compositingEnabled(bool enabled)
    {
    // Enable the other configuration tabs only when compositing is enabled.
    ui.compositingOptionsContainer->setEnabled(enabled);
    ui.tabWidget->setTabEnabled(1, enabled);
    ui.tabWidget->setTabEnabled(2, enabled);
    ui.tabWidget->setTabEnabled(3, enabled);
    }

void KWinCompositingConfig::initEffectSelector()
    {
    // Find all .desktop files of the effects
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QList<KPluginInfo> effectinfos = KPluginInfo::fromServices(offers);

    // Add them to the plugin selector
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Appearance"), "Appearance", mNewConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Accessibility"), "Accessibility", mNewConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Focus"), "Focus", mNewConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Window Management"), "Window Management", mNewConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Candy"), "Candy", mNewConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Demos"), "Demos", mNewConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tests"), "Tests", mNewConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tools"), "Tools", mNewConfig);
    }

void KWinCompositingConfig::currentTabChanged(int tab)
    {
    // block signals to don't emit the changed()-signal by just switching the current tab
    blockSignals(true);

    // write possible changes done to synchronize effect checkboxes and selector
    // TODO: This segment is prone to fail when the UI is changed;
    // you'll most likely not think of the hard coded numbers here when just changing the order of the tabs.
    if (tab == 0)
        {
        // General tab was activated
        saveEffectsTab();
        loadGeneralTab();
        }
    else if ( tab == 2 )
        {
        // Effects tab was activated
        saveGeneralTab();
        loadEffectsTab();
        }

    blockSignals(false);
    }

void KWinCompositingConfig::loadGeneralTab()
    {
    KConfigGroup config(mNewConfig, "Compositing");
    ui.useCompositing->setChecked(config.readEntry("Enabled", mDefaultPrefs.enableCompositing()));
    ui.animationSpeedCombo->setCurrentIndex(config.readEntry("AnimationSpeed", 3 ));

    // Load effect settings
    KConfigGroup effectconfig(mNewConfig, "Plugins");
#define LOAD_EFFECT_CONFIG(effectname)  effectconfig.readEntry("kwin4_effect_" effectname "Enabled", true)
    int winManagementEnabled = LOAD_EFFECT_CONFIG("presentwindows")
        + LOAD_EFFECT_CONFIG("desktopgrid")
        + LOAD_EFFECT_CONFIG("dialogparent");
    if (winManagementEnabled > 0 && winManagementEnabled < 3)
        {
        ui.effectWinManagement->setTristate(true);
        ui.effectWinManagement->setCheckState(Qt::PartiallyChecked);
        }
    else
        ui.effectWinManagement->setChecked(winManagementEnabled);
    ui.effectShadows->setChecked(LOAD_EFFECT_CONFIG("shadow"));
    ui.effectAnimations->setChecked(LOAD_EFFECT_CONFIG("minimizeanimation"));
#undef LOAD_EFFECT_CONFIG

    // window switching
    // Set current option to "none" if no plugin is activated.
    ui.windowSwitchingCombo->setCurrentIndex( 0 );
    if( effectEnabled( "boxswitch", effectconfig ))
        ui.windowSwitchingCombo->setCurrentIndex( 1 );
    if( effectEnabled( "coverswitch", effectconfig ))
        ui.windowSwitchingCombo->setCurrentIndex( 3 );
    if( effectEnabled( "flipswitch", effectconfig ))
        ui.windowSwitchingCombo->setCurrentIndex( 4 );
    KConfigGroup presentwindowsconfig(mNewConfig, "Effect-PresentWindows");
    if( effectEnabled( "presentwindows", effectconfig ) && presentwindowsconfig.readEntry("TabBox", false) )
        ui.windowSwitchingCombo->setCurrentIndex( 2 );

    // desktop switching
    // Set current option to "none" if no plugin is activated.
    ui.desktopSwitchingCombo->setCurrentIndex( 0 );
    if( effectEnabled( "slide", effectconfig ))
        ui.desktopSwitchingCombo->setCurrentIndex( 1 );
    KConfigGroup cubeconfig(mNewConfig, "Effect-Cube");
    if( effectEnabled( "cube", effectconfig ) && cubeconfig.readEntry("AnimateDesktopChange", false))
        ui.desktopSwitchingCombo->setCurrentIndex( 2 );
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

void KWinCompositingConfig::loadAdvancedTab()
    {
    KConfigGroup config(mNewConfig, "Compositing");
    QString backend = config.readEntry("Backend", "OpenGL");
    ui.compositingType->setCurrentIndex((backend == "XRender") ? 1 : 0);
    // 4 - off, 5 - shown, 6 - always, other are old values
    int hps = config.readEntry("HiddenPreviews", 5);
    if( hps == 6 ) // always
        ui.windowThumbnails->setCurrentIndex( 0 );
    else if( hps == 4 ) // never
        ui.windowThumbnails->setCurrentIndex( 2 );
    else // shown, or default
        ui.windowThumbnails->setCurrentIndex( 1 );
    ui.disableChecks->setChecked( config.readEntry( "DisableChecks", false ));

    QString glMode = config.readEntry("GLMode", "TFP");
    ui.glMode->setCurrentIndex((glMode == "TFP") ? 0 : ((glMode == "SHM") ? 1 : 2));
    ui.glTextureFilter->setCurrentIndex(config.readEntry("GLTextureFilter", 1));
    ui.glDirect->setChecked(config.readEntry("GLDirect", mDefaultPrefs.enableDirectRendering()));
    ui.glVSync->setChecked(config.readEntry("GLVSync", mDefaultPrefs.enableVSync()));

    ui.xrenderSmoothScale->setChecked(config.readEntry("XRenderSmoothScale", false));
    }

void KWinCompositingConfig::load()
    {
    // Discard any unsaved changes.
    resetNewToBackupConfig();

    loadGeneralTab();
    loadElectricBorders();
    loadEffectsTab();
    loadAdvancedTab();

    emit changed( false );
    }

void KWinCompositingConfig::saveGeneralTab()
    {
    KConfigGroup config(mNewConfig, "Compositing");

    config.writeEntry("Enabled", ui.useCompositing->isChecked());
    config.writeEntry("AnimationSpeed", ui.animationSpeedCombo->currentIndex());

    // Save effects
    KConfigGroup effectconfig( mNewConfig, "Plugins" );
#define WRITE_EFFECT_CONFIG(effectname, widget)  effectconfig.writeEntry("kwin4_effect_" effectname "Enabled", widget->isChecked())
    if (ui.effectWinManagement->checkState() != Qt::PartiallyChecked)
        {
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
    KConfigGroup presentwindowsconfig(mNewConfig, "Effect-PresentWindows");
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
    KConfigGroup cubeconfig(mNewConfig, "Effect-Cube");
    cubeconfig.writeEntry("AnimateDesktopChange", cubeDesktopSwitching);
    }

void KWinCompositingConfig::saveEffectsTab()
    {
    ui.effectSelector->save();
    }

void KWinCompositingConfig::saveAdvancedTab()
    {
    static const int hps[] = { 6 /*always*/, 5 /*shown*/,  4 /*never*/ };

    KConfigGroup config(mNewConfig, "Compositing");
    QString glModes[] = { "TFP", "SHM", "Fallback" };

    config.writeEntry("Backend", (ui.compositingType->currentIndex() == 0) ? "OpenGL" : "XRender");
    config.writeEntry("HiddenPreviews", hps[ ui.windowThumbnails->currentIndex() ] );
    config.writeEntry("DisableChecks", ui.disableChecks->isChecked());

    config.writeEntry("GLMode", glModes[ui.glMode->currentIndex()]);
    config.writeEntry("GLTextureFilter", ui.glTextureFilter->currentIndex());
    config.writeEntry("GLDirect", ui.glDirect->isChecked());
    config.writeEntry("GLVSync", ui.glVSync->isChecked());

    config.writeEntry("XRenderSmoothScale", ui.xrenderSmoothScale->isChecked());
    }

void KWinCompositingConfig::save()
    {
    // bah; tab content being dependent on the other is really bad; and
    // deprecated in the HIG for a reason.  Its confusing!
    // Make sure we only call save on each tab once; as they are stateful due to the revert concept
    if ( ui.tabWidget->currentIndex() == 0 ) // "General" tab was active
        {
        saveGeneralTab();
        loadEffectsTab();
        saveEffectsTab();
        }
    else
        {
        saveEffectsTab();
        loadGeneralTab();
        saveGeneralTab();
        }

    saveElectricBorders();
    saveAdvancedTab();

    emit changed( false );

    activateNewConfig();
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

    ui.compositingType->setCurrentIndex( 0 );
    ui.windowThumbnails->setCurrentIndex( 1 );
    ui.disableChecks->setChecked( false );
    ui.glMode->setCurrentIndex( 0 );
    ui.glTextureFilter->setCurrentIndex( 1 );
    ui.glDirect->setChecked( mDefaultPrefs.enableDirectRendering() );
    ui.glVSync->setChecked( mDefaultPrefs.enableVSync() );
    ui.xrenderSmoothScale->setChecked( false );

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
        addItemToEdgesMonitor( services.first()->name() + " - " + i18n( "All Desktops" ));
        addItemToEdgesMonitor( services.first()->name() + " - " + i18n( "Current Desktop" ));
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
    KConfigGroup presentwindowsconfig(mNewConfig, "Effect-PresentWindows");
    changeElectricBorder( (ElectricBorder)presentwindowsconfig.readEntry( "BorderActivateAll",
        int( ElectricTopLeft )), (int)PresentWindowsAll );
    changeElectricBorder( (ElectricBorder)presentwindowsconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)PresentWindowsCurrent );
    // Desktop Grid
    KConfigGroup gridconfig(mNewConfig, "Effect-DesktopGrid");
    changeElectricBorder( (ElectricBorder)gridconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)DesktopGrid );
    // Desktop Cube
    KConfigGroup cubeconfig(mNewConfig, "Effect-Cube");
    changeElectricBorder( (ElectricBorder)cubeconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)Cube );
    // Desktop Cylinder
    KConfigGroup cylinderconfig(mNewConfig, "Effect-Cylinder");
    changeElectricBorder( (ElectricBorder)cylinderconfig.readEntry( "BorderActivate",
        int( ElectricNone )), (int)Cylinder );
    // Desktop Grid
    KConfigGroup sphereconfig(mNewConfig, "Effect-Sphere");
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
    KConfigGroup presentwindowsconfig(mNewConfig, "Effect-PresentWindows");
    presentwindowsconfig.writeEntry( "BorderActivateAll", (int)checkEffectHasElectricBorder( (int)PresentWindowsAll ));
    presentwindowsconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)PresentWindowsCurrent ));

    KConfigGroup gridconfig(mNewConfig, "Effect-DesktopGrid");
    gridconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)DesktopGrid ));

    KConfigGroup cubeconfig(mNewConfig, "Effect-Cube");
    cubeconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)Cube ));

    KConfigGroup cylinderconfig(mNewConfig, "Effect-Cylinder");
    cylinderconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)Cylinder ));

    KConfigGroup sphereconfig(mNewConfig, "Effect-Sphere");
    sphereconfig.writeEntry( "BorderActivate", (int)checkEffectHasElectricBorder( (int)Sphere ));
    }

void KWinCompositingConfig::activateNewConfig()
    {
    // Write the backup file to disk so it can be used on the next KWin start
    // if KWin or the X server should crash while testing the new config.
    mBackupConfig->sync();

    bool configOk = true;

    // Tell KWin to load the new configuration.
    bool couldReload = sendKWinReloadSignal();
    // If it was not successfull, show an error message to the user.
    if ( !couldReload )
        {
        KMessageBox::sorry( this, i18n(
            "Failed to activate desktop effects using the given "
            "configuration options. Settings will be reverted to their previous values.\n\n"
            "Check your X configuration. You may also consider changing advanced options, "
            "especially changing the compositing type." ) );
        configOk = false;
        }
    else
       {
        // If the confirmation dialog should be shown, wait for its result.
        if ( isConfirmationNeeded() )
            {
            ConfirmDialog confirm;
            configOk = confirm.exec();
            }
        }

    if ( configOk )
        {
        // The new config is stable and was accepted.
        // Update our internal backup copy of the config.
        updateBackupWithNewConfig();
        }
    else
        {
        // Copy back the old config and load it into the ui.
        load();
        // Tell KWin to reload the (old) config.
        sendKWinReloadSignal();
        }

    // Remove the backup file from disk because it is the same as the main config
    // anyway and would only confuse the startup code if KWin is restarted while
    // this module is still opened, (e.g. by calling "kwin --replace").
    deleteBackupConfigFile();
    }

bool KWinCompositingConfig::isConfirmationNeeded() const
    {
    // Compare new config with the backup and see if any potentially dangerous
    // values have been changed.
    KConfigGroup newGroup( mNewConfig, "Compositing" );
    KConfigGroup backupGroup( mBackupConfig, "Compositing" );

    // Manually enabling compositing is potentially dangerous, but disabling is never.
    bool newEnabled = newGroup.readEntry( "Enabled", mDefaultPrefs.enableCompositing() );
    bool backupEnabled = backupGroup.readEntry( "Enabled", mDefaultPrefs.enableCompositing() );
    if ( newEnabled && !backupEnabled )
        {
        return true;
        }

    // All potentially dangerous options along with their (usually safe) default values.
    QHash<QString, QVariant> confirm;
    confirm[ "Backend" ] = "OpenGL";
    confirm[ "GLMode" ] = "TFP";
    confirm[ "GLDirect" ] = mDefaultPrefs.enableDirectRendering();
    confirm[ "GLVSync" ] = mDefaultPrefs.enableVSync();
    confirm[ "DisableChecks" ] = false;

    QHash<QString, QVariant>::const_iterator i = confirm.constBegin();
    for( ; i != confirm.constEnd(); ++i )
        {
        if ( newGroup.readEntry( i.key(), i.value() ) !=
            backupGroup.readEntry( i.key(), i.value() ) )
            {
            return true;
            }
        }

    // If we have not returned yet, no critical settings have been changed.
    return false;
    }

bool KWinCompositingConfig::sendKWinReloadSignal()
    {
    // Save the new config to the file system, thus allowing KWin to read it.
    mNewConfig->sync();

    // Send signal to all kwin instances.
    QDBusMessage message = QDBusMessage::createSignal( "/KWin", "org.kde.KWin", "reloadCompositingConfig" );
    QDBusConnection::sessionBus().send(message);

    //-------------
    // If we added or removed shadows we need to reload decorations as well
    // We have to do this separately so the settings are in sync
    // HACK: This should really just reload decorations, not do a full reconfigure

    KConfigGroup effectConfig;

    effectConfig = KConfigGroup( mBackupConfig, "Compositing" );
    bool enabledBefore = effectConfig.readEntry( "Enabled", mDefaultPrefs.enableCompositing() );
    effectConfig = KConfigGroup( mNewConfig, "Compositing" );
    bool enabledAfter = effectConfig.readEntry( "Enabled", mDefaultPrefs.enableCompositing() );

    effectConfig = KConfigGroup( mBackupConfig, "Plugins" );
    bool shadowBefore = effectEnabled( "shadow", effectConfig );
    effectConfig = KConfigGroup( mNewConfig, "Plugins" );
    bool shadowAfter = effectEnabled( "shadow", effectConfig );

    if( enabledBefore != enabledAfter || shadowBefore != shadowAfter )
        {
        message = QDBusMessage::createMethodCall( "org.kde.kwin", "/KWin", "org.kde.KWin", "reconfigure" );
        QDBusConnection::sessionBus().send( message );
        }

    //-------------

    // If compositing is enabled, check if it could be started.
    KConfigGroup newGroup( mNewConfig, "Compositing" );
    bool enabled = newGroup.readEntry( "Enabled", mDefaultPrefs.enableCompositing() );

    if ( !enabled )
        {
        return true;
        }
    else
        {
        // Feel free to extend this to support several kwin instances (multihead) if you
        // think it makes sense.
        OrgKdeKWinInterface kwin( "org.kde.kwin", "/KWin", QDBusConnection::sessionBus() );
        return kwin.compositingActive().value();
        }
    }

void KWinCompositingConfig::updateBackupWithNewConfig()
    {
    mNewConfig->copyTo( mBackupConfig->name(), mBackupConfig );
    }

void KWinCompositingConfig::resetNewToBackupConfig()
    {
    mBackupConfig->copyTo( mNewConfig->name(), mNewConfig.data() );
    }

void KWinCompositingConfig::deleteBackupConfigFile()
    {
    QString backupFileName = KStandardDirs::locate( "config", mBackupConfig->name() );
    // If the file does not exist, QFile::remove() just fails silently.
    QFile::remove( backupFileName );
    }

} // namespace

#include "main.moc"

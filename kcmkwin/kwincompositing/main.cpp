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
#include <kdebug.h>
#include <kmessagebox.h>
#include <ksettings/dispatcher.h>
#include <kpluginselector.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <kservice.h>
#include <ktitlewidget.h>
#include <knotification.h>

#include <QtDBus/QtDBus>
#include <QTimer>
#include <QLabel>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KIcon>

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

    setWindowIcon(KIcon("preferences-desktop-effect"));
    }


KWinCompositingConfig::KWinCompositingConfig(QWidget *parent, const QVariantList &)
    : KCModule( KWinCompositingConfigFactory::componentData(), parent )
    , mKWinConfig(KSharedConfig::openConfig( "kwinrc" ))
    , m_showConfirmDialog( false )
    , kwinInterface( NULL )
    {
    KGlobal::locale()->insertCatalog( "kwin_effects" );
    ui.setupUi(this);
    layout()->setMargin(0);
    ui.verticalSpacer->changeSize(20, KDialog::groupSpacingHint());
    ui.verticalSpacer_2->changeSize(20, KDialog::groupSpacingHint());
    ui.tabWidget->setCurrentIndex(0);
    ui.statusTitleWidget->hide();

#define OPENGL_INDEX 0
#define XRENDER_INDEX 1
#ifndef KWIN_HAVE_OPENGL_COMPOSITING
    ui.compositingType->removeItem( OPENGL_INDEX );
    ui.glGroup->setEnabled( false );
#define OPENGL_INDEX -1
#define XRENDER_INDEX 0
#endif
#ifndef KWIN_HAVE_XRENDER_COMPOSITING
    ui.compositingType->removeItem( XRENDER_INDEX );
    ui.xrenderGroup->setEnabled( false );
#define XRENDER_INDEX -1
#endif

    kwinInterface = new OrgKdeKWinInterface( "org.kde.kwin", "/KWin", QDBusConnection::sessionBus() );

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

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.windowThumbnails, SIGNAL(activated(int)), this, SLOT(changed()));
    connect(ui.disableChecks, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glTextureFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glDirect, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glVSync, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.xrenderSmoothScale, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.compositingStateButton, SIGNAL(clicked(bool)), kwinInterface, SLOT(toggleCompositing()));
    connect(kwinInterface, SIGNAL(compositingToggled(bool)), this, SLOT(setupCompositingState(bool)));

    // NOTICE: this is intended to workaround broken GL implementations that succesfully segfault on glXQuery :-(
    KConfigGroup gl_workaround_config(mKWinConfig, "Compositing");
    bool checkIsSave = gl_workaround_config.readEntry("CheckIsSafe", true);
    gl_workaround_config.writeEntry("CheckIsSafe", false);
    gl_workaround_config.sync();
    
    // Open the temporary config file
    // Temporary conf file is used to synchronize effect checkboxes with effect
    // selector by loading/saving effects from/to temp config when active tab
    // changes.
    mTmpConfigFile.open();
    mTmpConfig = KSharedConfig::openConfig(mTmpConfigFile.fileName());

    if( checkIsSave && CompositingPrefs::compositingPossible() )
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

        setupCompositingState( false, false );
        }

    gl_workaround_config.writeEntry("CheckIsSafe", checkIsSave);
    gl_workaround_config.sync();

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwincompositing"), 0,
            ki18n("KWin Desktop Effects Configuration Module"),
            0, KLocalizedString(), KAboutData::License_GPL, ki18n("(c) 2007 Rivo Laks"));
    about->addAuthor(ki18n("Rivo Laks"), KLocalizedString(), "rivolaks@hot.ee");
    setAboutData(about);

    // search the effect names
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    QString boxswitch, presentwindows, coverswitch, flipswitch, slide, cube, fadedesktop;
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
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_cubeslide'");
    if( !services.isEmpty() )
        cube = services.first()->name();
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_fadedesktop'");
    if( !services.isEmpty() )
        fadedesktop = services.first()->name();
    // init the combo boxes
    ui.windowSwitchingCombo->addItem(i18n("No Effect"));
    ui.windowSwitchingCombo->addItem(boxswitch);
    ui.windowSwitchingCombo->addItem(presentwindows);
    ui.windowSwitchingCombo->addItem(coverswitch);
    ui.windowSwitchingCombo->addItem(flipswitch);

    ui.desktopSwitchingCombo->addItem(i18n("No Effect"));
    ui.desktopSwitchingCombo->addItem(slide);
    ui.desktopSwitchingCombo->addItem(cube);
    ui.desktopSwitchingCombo->addItem(fadedesktop);
    }

KWinCompositingConfig::~KWinCompositingConfig()
    {
    }

void KWinCompositingConfig::reparseConfiguration( const QByteArray& conf )
    {
    KSettings::Dispatcher::reparseConfiguration( conf );
    }

void KWinCompositingConfig::compositingEnabled( bool enabled )
    {
    // Enable the other configuration tabs only when compositing is enabled.
    ui.compositingOptionsContainer->setEnabled(enabled);
    ui.tabWidget->setTabEnabled(1, enabled);
    ui.tabWidget->setTabEnabled(2, enabled);
    }

void KWinCompositingConfig::showConfirmDialog( bool reinitCompositing )
    {
    bool revert = false;
    // Feel free to extend this to support several kwin instances (multihead) if you
    // think it makes sense.
    OrgKdeKWinInterface kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
    if( reinitCompositing ? !kwin.compositingActive().value() : !kwin.waitForCompositingSetup().value() )
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
        else
            {
            // compositing is enabled now
            setupCompositingState( kwinInterface->compositingActive() );
            checkLoadedEffects();
            }
        }
    if( revert )
        {
        // Revert settings
        KConfigGroup config(mKWinConfig, "Compositing");
        config.deleteGroup();
        QMap<QString, QString>::const_iterator it = mPreviousConfig.constBegin();
        for(; it != mPreviousConfig.constEnd(); ++it)
            {
            if (it.value().isEmpty())
                continue;
            config.writeEntry(it.key(), it.value());
            }
        // Sync with KWin and reload
        configChanged(reinitCompositing);
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
    // TODO: This segment is prone to fail when the UI is changed;
    // you'll most likely not think of the hard coded numbers here when just changing the order of the tabs.
    if (tab == 0)
        {
        // General tab was activated
        saveEffectsTab();
        loadGeneralTab();
        }
    else if ( tab == 1 )
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
    bool enabled = config.readEntry("Enabled", mDefaultPrefs.enableCompositing());
    ui.useCompositing->setChecked( enabled );
    ui.animationSpeedCombo->setCurrentIndex(config.readEntry("AnimationSpeed", 3 ));

    // Load effect settings
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
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
    KConfigGroup boxswitchconfig(mKWinConfig, "Effect-BoxSwitch");
    if( effectEnabled( "boxswitch", effectconfig ) && boxswitchconfig.readEntry("TabBox", true))
        ui.windowSwitchingCombo->setCurrentIndex( 1 );
    KConfigGroup coverswitchconfig(mKWinConfig, "Effect-CoverSwitch");
    if( effectEnabled( "coverswitch", effectconfig ) && coverswitchconfig.readEntry("TabBox", false))
        ui.windowSwitchingCombo->setCurrentIndex( 3 );
    KConfigGroup flipswitchconfig(mKWinConfig, "Effect-FlipSwitch");
    if( effectEnabled( "flipswitch", effectconfig ) && flipswitchconfig.readEntry("TabBox", false))
        ui.windowSwitchingCombo->setCurrentIndex( 4 );
    KConfigGroup presentwindowsconfig(mKWinConfig, "Effect-PresentWindows");
    if( effectEnabled( "presentwindows", effectconfig ) && presentwindowsconfig.readEntry("TabBox", false) )
        ui.windowSwitchingCombo->setCurrentIndex( 2 );

    // desktop switching
    // Set current option to "none" if no plugin is activated.
    ui.desktopSwitchingCombo->setCurrentIndex( 0 );
    if( effectEnabled( "slide", effectconfig ))
        ui.desktopSwitchingCombo->setCurrentIndex( 1 );
    if( effectEnabled( "cubeslide", effectconfig ))
        ui.desktopSwitchingCombo->setCurrentIndex( 2 );
    if( effectEnabled( "fadedesktop", effectconfig ))
        ui.desktopSwitchingCombo->setCurrentIndex( 3 );

    if( enabled )
        setupCompositingState( kwinInterface->compositingActive() );
    else
        setupCompositingState( false, false );
    }

void KWinCompositingConfig::setupCompositingState( bool active, bool enabled )
    {
    if( !qgetenv( "KDE_FAILSAFE" ).isNull() )
        enabled = false;
    // compositing state
    QString stateIcon;
    QString stateText;
    QString stateButtonText;
    if( enabled )
        {
        // check if compositing is active or suspended
        if( active )
            {
            stateIcon = QString( "dialog-ok-apply" );
            stateText = i18n( "Compositing is active" );
            stateButtonText = i18n( "Suspend Compositing" );
            }
        else
            {
            stateIcon = QString( "dialog-cancel" );
            stateText = i18n( "Compositing is temporarily disabled" );
            stateButtonText = i18n( "Resume Compositing" );
            }
        }
    else
        {
        // compositing is disabled
        stateIcon = QString( "dialog-cancel" );
        stateText = i18n( "Compositing is disabled" );
        stateButtonText = i18n( "Resume Compositing" );
        }
    const int iconSize = (QApplication::fontMetrics().height() > 24) ? 32 : 22;
    ui.compositingStateIcon->setPixmap( KIcon( stateIcon ).pixmap( iconSize, iconSize ) );
    ui.compositingStateLabel->setText( stateText );
    ui.compositingStateButton->setText( stateButtonText );
    ui.compositingStateIcon->setEnabled( enabled );
    ui.compositingStateLabel->setEnabled( enabled );
    ui.compositingStateButton->setEnabled( enabled );
    }

bool KWinCompositingConfig::effectEnabled( const QString& effect, const KConfigGroup& cfg ) const
    {
    KService::List services = KServiceTypeTrader::self()->query(
        "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
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
    KConfigGroup config(mKWinConfig, "Compositing");
    QString backend = config.readEntry("Backend", "OpenGL");
    ui.compositingType->setCurrentIndex((backend == "XRender") ? XRENDER_INDEX : 0);
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
    mKWinConfig->reparseConfiguration();

    // Copy Plugins group to temp config file
    QMap<QString, QString> entries = mKWinConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup tmpconfig(mTmpConfig, "Plugins");
    tmpconfig.deleteGroup();
    for(; it != entries.constEnd(); ++it)
        tmpconfig.writeEntry(it.key(), it.value());

    loadGeneralTab();
    loadEffectsTab();
    loadAdvancedTab();

    emit changed( false );
    }

void KWinCompositingConfig::saveGeneralTab()
    {
    KConfigGroup config(mKWinConfig, "Compositing");
    // Check if any critical settings that need confirmation have changed
    if(ui.useCompositing->isChecked() &&
       ui.useCompositing->isChecked() != config.readEntry("Enabled", mDefaultPrefs.enableCompositing()))
        m_showConfirmDialog = true;

    config.writeEntry("Enabled", ui.useCompositing->isChecked());
    config.writeEntry("AnimationSpeed", ui.animationSpeedCombo->currentIndex());

    // disable the compositing state if compositing was turned off
    if( !ui.useCompositing->isChecked() )
        setupCompositingState( false, false );

    // Save effects
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
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
    bool boxSwitch              = false;
    bool presentWindowSwitching = false;
    bool coverSwitch            = false;
    bool flipSwitch             = false;
    switch( windowSwitcher )
        {
        case 1:
            boxSwitch = true;
            break;
        case 2:
            presentWindowSwitching = true;
            break;
        case 3:
            coverSwitch = true;
            break;
        case 4:
            flipSwitch = true;
            break;
        default:
            break; // nothing
        }
    // activate effects if not active
    if( boxSwitch )
        effectconfig.writeEntry("kwin4_effect_boxswitchEnabled", true);
    if( presentWindowSwitching )
        effectconfig.writeEntry("kwin4_effect_presentwindowsEnabled", true);
    if( coverSwitch )
        effectconfig.writeEntry("kwin4_effect_coverswitchEnabled", true);
    if( flipSwitch )
        effectconfig.writeEntry("kwin4_effect_flipswitchEnabled", true);
    KConfigGroup boxswitchconfig( mKWinConfig, "Effect-BoxSwitch" );
    boxswitchconfig.writeEntry( "TabBox", boxSwitch );
    boxswitchconfig.sync();
    KConfigGroup presentwindowsconfig( mKWinConfig, "Effect-PresentWindows" );
    presentwindowsconfig.writeEntry( "TabBox", presentWindowSwitching );
    presentwindowsconfig.sync();
    KConfigGroup coverswitchconfig( mKWinConfig, "Effect-CoverSwitch" );
    coverswitchconfig.writeEntry( "TabBox", coverSwitch );
    coverswitchconfig.sync();
    KConfigGroup flipswitchconfig( mKWinConfig, "Effect-FlipSwitch" );
    flipswitchconfig.writeEntry( "TabBox", flipSwitch );
    flipswitchconfig.sync();

    int desktopSwitcher = ui.desktopSwitchingCombo->currentIndex();
    switch( desktopSwitcher )
        {
        case 0:
            // no effect
            effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
            effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", false);
            effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", false);
            break;
        case 1:
            // slide
            effectconfig.writeEntry("kwin4_effect_slideEnabled", true);
            effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", false);
            effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", false);
            break;
        case 2:
            // cube
            effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
            effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", true);
            effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", false);
            break;
        case 3:
            // fadedesktop
            effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
            effectconfig.writeEntry("kwin4_effect_cubeslideEnabled", false);
            effectconfig.writeEntry("kwin4_effect_fadedesktopEnabled", true);
            break;
        }
    }

void KWinCompositingConfig::saveEffectsTab()
    {
    ui.effectSelector->save();
    }

bool KWinCompositingConfig::saveAdvancedTab()
    {
    bool advancedChanged = false;
    static const int hps[] = { 6 /*always*/, 5 /*shown*/,  4 /*never*/ };

    KConfigGroup config(mKWinConfig, "Compositing");
    QString glModes[] = { "TFP", "SHM", "Fallback" };

    if( config.readEntry("Backend", "OpenGL")
            != ((ui.compositingType->currentIndex() == 0) ? "OpenGL" : "XRender")
        || config.readEntry("GLMode", "TFP") != glModes[ui.glMode->currentIndex()]
        || config.readEntry("GLDirect", mDefaultPrefs.enableDirectRendering())
            != ui.glDirect->isChecked()
        || config.readEntry("GLVSync", mDefaultPrefs.enableVSync()) != ui.glVSync->isChecked()
        || config.readEntry("DisableChecks", false ) != ui.disableChecks->isChecked())
        {
        m_showConfirmDialog = true;
        advancedChanged = true;
        }
    else if( config.readEntry("HiddenPreviews", 5) != hps[ ui.windowThumbnails->currentIndex() ]
        || config.readEntry("XRenderSmoothScale", false ) != ui.xrenderSmoothScale->isChecked() )
        advancedChanged = true;

    config.writeEntry("Backend", (ui.compositingType->currentIndex() == OPENGL_INDEX) ? "OpenGL" : "XRender");
    config.writeEntry("HiddenPreviews", hps[ ui.windowThumbnails->currentIndex() ] );
    config.writeEntry("DisableChecks", ui.disableChecks->isChecked());

    config.writeEntry("GLMode", glModes[ui.glMode->currentIndex()]);
    config.writeEntry("GLTextureFilter", ui.glTextureFilter->currentIndex());
    config.writeEntry("GLDirect", ui.glDirect->isChecked());
    config.writeEntry("GLVSync", ui.glVSync->isChecked());

    config.writeEntry("XRenderSmoothScale", ui.xrenderSmoothScale->isChecked());

    return advancedChanged;
    }

void KWinCompositingConfig::save()
    {
    // Save current config. We'll use this for restoring in case something goes wrong.
    KConfigGroup config(mKWinConfig, "Compositing");
    mPreviousConfig = config.entryMap();

    // bah; tab content being dependent on the other is really bad; and
    // deprecated in the HIG for a reason.  It is confusing!
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
    bool advancedChanged = saveAdvancedTab();

    // Copy Plugins group from temp config to real config
    QMap<QString, QString> entries = mTmpConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup realconfig(mKWinConfig, "Plugins");
    realconfig.deleteGroup();
    for(; it != entries.constEnd(); ++it)
        realconfig.writeEntry(it.key(), it.value());

    emit changed( false );

    configChanged(advancedChanged);

    // This assumes that this KCM is running with the same environment variables as KWin
    // TODO: Detect KWIN_COMPOSE=N as well
    if( !qgetenv( "KDE_FAILSAFE" ).isNull() && ui.useCompositing->isChecked() )
        {
        KMessageBox::sorry( this, i18n(
            "Your settings have been saved but as KDE is currently running in failsafe "
            "mode desktop effects cannot be enabled at this time.\n\n"
            "Please exit failsafe mode to enable desktop effects." ));
        m_showConfirmDialog = false; // Dangerous but there is no way to test if failsafe mode
        }

    if(m_showConfirmDialog)
        {
        m_showConfirmDialog = false;
        showConfirmDialog(advancedChanged);
        }
    }

void KWinCompositingConfig::checkLoadedEffects()
    {
    // check for effects not supported by Backend or hardware
    // such effects are enabled but not returned by DBus method loadedEffects
    QDBusMessage message = QDBusMessage::createMethodCall( "org.kde.kwin", "/KWin", "org.kde.KWin", "loadedEffects" );
    QDBusMessage reply = QDBusConnection::sessionBus().call( message );
    KConfigGroup effectConfig = KConfigGroup( mKWinConfig, "Compositing" );
    bool enabledAfter = effectConfig.readEntry( "Enabled", mDefaultPrefs.enableCompositing() );

    if( reply.type() == QDBusMessage::ReplyMessage && enabledAfter && !getenv( "KDE_FAILSAFE" ))
        {
        effectConfig = KConfigGroup( mKWinConfig, "Plugins" );
        QStringList loadedEffects = reply.arguments()[0].toStringList();
        QStringList effects = effectConfig.keyList();
        QStringList disabledEffects = QStringList();
        foreach( QString effect, effects ) // krazy:exclude=foreach
            {
            QString temp = effect.mid( 13, effect.length() - 13 - 7 );
            effect.truncate( effect.length() - 7 );
            if( effectEnabled( temp, effectConfig ) && !loadedEffects.contains( effect ) )
                {
                disabledEffects << effect;
                }
            }
        if( !disabledEffects.isEmpty() )
            {
            KServiceTypeTrader* trader = KServiceTypeTrader::self();
            KService::List services;
            QString message = i18n( "The following effects could not be activated:" );
            message.append( "<ul>" );
            foreach( const QString &effect, disabledEffects )
                {
                services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == '" + effect + '\'');
                message.append( "<li>" );
                if( !services.isEmpty() )
                    message.append( services.first()->name() );
                else
                    message.append( effect);
                message.append( "</li>" );
                }
            message.append( "</ul>" );
            KNotification::event( "effectsnotsupported", message, QPixmap(), NULL, KNotification::CloseOnTimeout, KComponentData( "kwin" ) );
            }
        }
    }

void KWinCompositingConfig::configChanged(bool reinitCompositing)
    {
    // Send signal to kwin
    mKWinConfig->sync();
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin",
        reinitCompositing ? "reinitCompositing" : "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    // HACK: We can't just do this here, due to the asynchronous nature of signals.
    // We also can't change reinitCompositing into a message (which would allow
    // callWithCallbac() to do this neater) due to multiple kwin instances.
    if( !m_showConfirmDialog )
        QTimer::singleShot(1000, this, SLOT(checkLoadedEffects()));
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
    }

QString KWinCompositingConfig::quickHelp() const
    {
    return i18n("<h1>Desktop Effects</h1>");
    }

} // namespace

#include "main.moc"

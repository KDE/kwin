/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "main.h"

#include "advanced.h"

#include <kaboutdata.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <ksettings/dispatcher.h>
#include <kpluginselector.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <kservice.h>

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
        KDialog()
    {
    setCaption( i18n( "Desktop effects settings changed" ));
    setButtons( KDialog::Yes | KDialog::No );
    setDefaultButton(KDialog::No);
    setButtonFocus(KDialog::No);

    mTextLabel = new QLabel(this);
    setMainWidget(mTextLabel);

    mSecondsToLive = 10+1;
    advanceTimer();
    }

void ConfirmDialog::advanceTimer()
    {
    mSecondsToLive--;
    if(mSecondsToLive > 0)
    {
        QString text = i18n("Desktop effects settings have changed.\n"
                "Do you want to keep the new settings?\n"
                "They will be automatically reverted in %1 seconds", mSecondsToLive);
        mTextLabel->setText(text);
        QTimer::singleShot(1000, this, SLOT(advanceTimer()));
    }
    else
    {
        slotButtonClicked(KDialog::No);
    }
}


KWinCompositingConfig::KWinCompositingConfig(QWidget *parent, const QVariantList &)
    : KCModule( KWinCompositingConfigFactory::componentData(), parent),
        mKWinConfig(KSharedConfig::openConfig("kwinrc"))
{
    ui.setupUi(this);
    ui.tabWidget->setCurrentIndex(0);
    ui.statusLabel->hide();

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

    // Open the temporary config file
    // Temporary conf file is used to syncronize effect checkboxes with effect
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

        ui.statusLabel->setText(i18n("Compositing is not supported on your system."));
        ui.statusLabel->show();
    }

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwincompositing"), 0,
            ki18n("KWin Desktop Effects Configuration Module"),
            0, KLocalizedString(), KAboutData::License_GPL, ki18n("(c) 2007 Rivo Laks"));
    about->addAuthor(ki18n("Rivo Laks"), KLocalizedString(), "rivolaks@hot.ee");
    setAboutData(about);
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
    ConfirmDialog confirm;
    int result = confirm.exec();
    if(result != KDialog::Yes)
    {
        // Revert settings
        KConfigGroup config(mKWinConfig, "Compositing");
        config.deleteGroup();
        QMap<QString, QString>::const_iterator i = mPreviousConfig.constBegin();
        while (i != mPreviousConfig.constEnd()) {
            config.writeEntry(i.key(), i.value());
            ++i;
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
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Window Management"), "Window Management", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Demos"), "Demos", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tests"), "Tests", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Misc"), "Misc", mTmpConfig);
}

void KWinCompositingConfig::currentTabChanged(int tab)
{
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
}

void KWinCompositingConfig::loadGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    ui.useCompositing->setChecked(config.readEntry("Enabled", mDefaultPrefs.enableCompositing()));

    // Load effect settings
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define LOAD_EFFECT_CONFIG(effectname)  effectconfig.readEntry("kwin4_effect_" effectname "Enabled", true)
    bool winManagementEnabled = LOAD_EFFECT_CONFIG("presentwindows");
    winManagementEnabled &= LOAD_EFFECT_CONFIG("boxswitch");
    winManagementEnabled &= LOAD_EFFECT_CONFIG("desktopgrid");
    winManagementEnabled &= LOAD_EFFECT_CONFIG("dialogparent");
    ui.effectWinManagement->setChecked(winManagementEnabled);
    ui.effectShadows->setChecked(LOAD_EFFECT_CONFIG("shadow"));
    ui.effectAnimations->setChecked(LOAD_EFFECT_CONFIG("minimizeanimation"));
#undef LOAD_EFFECT_CONFIG
}

void KWinCompositingConfig::loadEffectsTab()
{
    ui.effectSelector->load();
}

void KWinCompositingConfig::load()
{
    kDebug() ;
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

bool KWinCompositingConfig::saveGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    // Save current config. We'll use this for restoring in case something
    //  goes wrong.
    mPreviousConfig = config.entryMap();
    // Check if any critical settings that need confirmation have changed
    bool confirm = false;
    confirm |= (ui.useCompositing->isChecked() != config.readEntry("Enabled", mDefaultPrefs.enableCompositing()));

    config.writeEntry("Enabled", ui.useCompositing->isChecked());

    // Save effects
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define WRITE_EFFECT_CONFIG(effectname, widget)  effectconfig.writeEntry("kwin4_effect_" effectname "Enabled", widget->isChecked())
    WRITE_EFFECT_CONFIG("presentwindows", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("boxswitch", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("desktopgrid", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("dialogparent", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("shadow", ui.effectShadows);
    // TODO: maybe also do some effect-specific configuration here, e.g.
    //  enable/disable desktopgrid's animation according to this setting
    WRITE_EFFECT_CONFIG("minimizeanimation", ui.effectAnimations);
#undef WRITE_EFFECT_CONFIG

    return confirm;
}

void KWinCompositingConfig::saveEffectsTab()
{
    ui.effectSelector->save();
}

void KWinCompositingConfig::save()
{
    kDebug() ;

    bool confirm = saveGeneralTab();
    saveEffectsTab();

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

    if(confirm)
    {
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
    kDebug() ;
    ui.useCompositing->setChecked(mDefaultPrefs.enableCompositing());
    ui.effectWinManagement->setChecked(true);
    ui.effectShadows->setChecked(true);
    ui.effectAnimations->setChecked(true);

    ui.effectSelector->defaults();
}

QString KWinCompositingConfig::quickHelp() const
{
    kDebug() ;
    return i18n("<h1>Desktop Effects</h1>");
}

} // namespace

#include "main.moc"

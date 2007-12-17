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
        KTimerDialog(10000, KTimerDialog::CountDown, 0, "mainKTimerDialog", true,
                     i18n("Confirm Desktop Effects Change"), KTimerDialog::Ok|KTimerDialog::Cancel,
                     KTimerDialog::Cancel)
    {
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
        mKWinConfig(KSharedConfig::openConfig("kwinrc"))
{
    KGlobal::locale()->insertCatalog( "kwin_effects" );
    ui.setupUi(this);
    layout()->setMargin(0);
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

        QString text = i18n("Compositing is not supported on your system.");
        text += "<br><br>";
        text += CompositingPrefs::compositingNotPossibleReason();
        ui.statusLabel->setText(text);
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
    if(!confirm.exec())
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
    int winManagementEnabled = LOAD_EFFECT_CONFIG("presentwindows")
        + LOAD_EFFECT_CONFIG("boxswitch")
        + LOAD_EFFECT_CONFIG("desktopgrid")
        + LOAD_EFFECT_CONFIG("dialogparent");
qDebug() << "winManagementEnabled" << winManagementEnabled;
    if (winManagementEnabled > 0 && winManagementEnabled < 4) {
        ui.effectWinManagement->setTristate(true);
        ui.effectWinManagement->setCheckState(Qt::PartiallyChecked);
    }
    else
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
    if(ui.useCompositing->isChecked()
        && ui.useCompositing->isChecked() != config.readEntry("Enabled", mDefaultPrefs.enableCompositing()))
        {
        confirm = true;
        }

    config.writeEntry("Enabled", ui.useCompositing->isChecked());

    // Save effects
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define WRITE_EFFECT_CONFIG(effectname, widget)  effectconfig.writeEntry("kwin4_effect_" effectname "Enabled", widget->isChecked())
    if (ui.effectWinManagement->checkState() != Qt::PartiallyChecked) {
qDebug() << "partial!";
        WRITE_EFFECT_CONFIG("presentwindows", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("boxswitch", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("desktopgrid", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("dialogparent", ui.effectWinManagement);
    }
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

    // Sync tabs. Otherwise effect tab will overwrite changes of general tab
    //  unless user manually switches tabs before saving
    currentTabChanged(ui.tabWidget->currentIndex() == 0 ? 1 : 0);

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

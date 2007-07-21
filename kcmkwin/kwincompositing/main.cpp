/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "main.h"

#include "advanced.h"

#include <kgenericfactory.h>
#include <kaboutdata.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <ksettings/dispatcher.h>

#include <QtDBus/QtDBus>




typedef KGenericFactory<KWin::KWinCompositingConfig> KWinCompositingConfigFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_kwincompositing, KWinCompositingConfigFactory("kcmkwincompositing"))


namespace KWin
{


KWinCompositingConfig::KWinCompositingConfig(QWidget *parent, const QStringList &)
    : KCModule( KWinCompositingConfigFactory::componentData(), parent),
        mKWinConfig(KSharedConfig::openConfig("kwinrc"))
{
    ui.setupUi(this);

    connect(ui.advancedOptions, SIGNAL(clicked()), this, SLOT(showAdvancedOptions()));
    connect(ui.useCompositing, SIGNAL(toggled(bool)), ui.compositingOptionsContainer, SLOT(setEnabled(bool)));

    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectWinManagement, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectShadows, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectAnimations, SIGNAL(toggled(bool)), this, SLOT(changed()));

    // Load config
    load();

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

void KWinCompositingConfig::showAdvancedOptions()
{
    KWinAdvancedCompositingOptions* dialog = new KWinAdvancedCompositingOptions(this, mKWinConfig);

    dialog->show();
    connect(dialog, SIGNAL(configSaved()), this, SLOT(configChanged()));
}

void KWinCompositingConfig::load()
{
    kDebug() << k_funcinfo << endl;
    mKWinConfig->reparseConfiguration();

    KConfigGroup config(mKWinConfig, "Compositing");
    ui.useCompositing->setChecked(config.readEntry("Enabled", false));

    // Load effect settings
    config.changeGroup("Plugins");
#define LOAD_EFFECT_CONFIG(effectname)  config.readEntry("kwin4_effect_" effectname "Enabled", true)
    bool winManagementEnabled = LOAD_EFFECT_CONFIG("presentwindows");
    winManagementEnabled &= LOAD_EFFECT_CONFIG("boxswitch");
    winManagementEnabled &= LOAD_EFFECT_CONFIG("desktopgrid");
    winManagementEnabled &= LOAD_EFFECT_CONFIG("dialogparent");
    ui.effectWinManagement->setChecked(winManagementEnabled);
    ui.effectShadows->setChecked(LOAD_EFFECT_CONFIG("shadow"));
    ui.effectAnimations->setChecked(LOAD_EFFECT_CONFIG("minimizeanimation"));

    emit changed( false );
}


void KWinCompositingConfig::save()
{
    kDebug() << k_funcinfo << endl;

    KConfigGroup config(mKWinConfig, "Compositing");
    config.writeEntry("Enabled", ui.useCompositing->isChecked());

    // Save effects
    config.changeGroup("Plugins");
#define WRITE_EFFECT_CONFIG(effectname, widget)  config.writeEntry("kwin4_effect_" effectname "Enabled", widget->isChecked())
    WRITE_EFFECT_CONFIG("presentwindows", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("boxswitch", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("desktopgrid", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("dialogparent", ui.effectWinManagement);
    WRITE_EFFECT_CONFIG("shadow", ui.effectShadows);
    // TODO: maybe also do some effect-specific configuration here, e.g.
    //  enable/disable desktopgrid's animation according to this setting
    WRITE_EFFECT_CONFIG("minimizeanimation", ui.effectAnimations);
#undef WRITE_EFFECT_CONFIG

    emit changed( false );

    configChanged();
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
    kDebug() << k_funcinfo << endl;
    ui.useCompositing->setChecked(false);
    ui.effectWinManagement->setChecked(true);
    ui.effectShadows->setChecked(true);
    ui.effectAnimations->setChecked(true);
}

QString KWinCompositingConfig::quickHelp() const
{
    kDebug() << k_funcinfo << endl;
    return i18n("<h1>Desktop Effects</h1>");
}

} // namespace

#include "main.moc"

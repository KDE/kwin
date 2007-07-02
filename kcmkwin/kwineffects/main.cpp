/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "main.h"

#include <kgenericfactory.h>
#include <kaboutdata.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kpluginselector.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <kservice.h>
#include <ksettings/dispatcher.h>

#include <QtDBus/QtDBus>
#include <QBoxLayout>




typedef KGenericFactory<KWin::KWinEffectsConfig> KWinEffectsConfigFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_kwineffects, KWinEffectsConfigFactory("kcmkwineffects"))


namespace KWin
{


KWinEffectsConfig::KWinEffectsConfig(QWidget *parent, const QStringList &)
    : KCModule( KWinEffectsConfigFactory::componentData(), parent),
        mKWinConfig(KSharedConfig::openConfig("kwinrc"))
{
    mPluginSelector = new KPluginSelector(this);
    QHBoxLayout* layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->addWidget(mPluginSelector);
    setLayout(layout);

    connect(mPluginSelector, SIGNAL(changed(bool)), this, SLOT(changed()));
    connect(mPluginSelector, SIGNAL(configCommitted(const QByteArray&)),
            this, SLOT(reparseConfiguration(const QByteArray&)));

    // Find all .desktop files of the effects
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QList<KPluginInfo*> effectinfos = KPluginInfo::fromServices(offers);

    // Add them to the plugin selector
    mPluginSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Appearance"), "Appearance", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Window Management"), "Window Management", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Demos"), "Demos", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tests"), "Tests", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Misc"), "Misc", mKWinConfig);

    // Load config
    load();

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwineffects"), 0,
            ki18n("Window Effects Configuration Module"),
            0, KLocalizedString(), KAboutData::License_GPL, ki18n("(c) 2007 Rivo Laks"));
    about->addAuthor(ki18n("Rivo Laks"), KLocalizedString(), "rivolaks@hot.ee");
    setAboutData(about);
}

KWinEffectsConfig::~KWinEffectsConfig()
{
}

void KWinEffectsConfig::reparseConfiguration(const QByteArray&conf)
{
  KSettings::Dispatcher::reparseConfiguration(conf);
}

void KWinEffectsConfig::load()
{
    kDebug() << k_funcinfo << endl;
    mKWinConfig->reparseConfiguration();

    mPluginSelector->load();

    emit changed( false );
}


void KWinEffectsConfig::save()
{
    kDebug() << k_funcinfo << endl;

    mPluginSelector->save();

    emit changed( false );

    // Send signal to kwin
    mKWinConfig->sync();
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}


void KWinEffectsConfig::defaults()
{
    kDebug() << k_funcinfo << endl;
    mPluginSelector->defaults();
}

QString KWinEffectsConfig::quickHelp() const
{
    kDebug() << k_funcinfo << endl;
    return i18n("<h1>Window Effects</h1> Here you can configure which effects will be used.");
}

} // namespace

#include "main.moc"

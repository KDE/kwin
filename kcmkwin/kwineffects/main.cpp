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

#include <QtDBus/QtDBus>
#include <QBoxLayout>
#include <QStringList>




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
    layout->addWidget(mPluginSelector);
    setLayout(layout);

    connect(mPluginSelector, SIGNAL(changed(bool)), this, SLOT(changed()));

    // Find all .desktop files of the effects
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QList<KPluginInfo*> effectinfos = KPluginInfo::fromServices(offers);

    // Add them to the plugin selector
    mPluginSelector->addPlugins(effectinfos, i18n("Appearance"), "Appearance", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, i18n("Window Management"), "Window Management", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, i18n("Demos"), "Demos", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, i18n("Tests"), "Tests", mKWinConfig);
    mPluginSelector->addPlugins(effectinfos, i18n("Misc"), "Misc", mKWinConfig);

    // Load config
    load();

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwineffects"),
            I18N_NOOP("Window Effects Configuration Module"),
            0, 0, KAboutData::License_GPL, I18N_NOOP("(c) 2007 Rivo Laks"));
    about->addAuthor("Rivo Laks", 0, "rivolaks@hot.ee");
    setAboutData(about);
}

KWinEffectsConfig::~KWinEffectsConfig()
{
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

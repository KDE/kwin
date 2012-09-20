/*
 *
 * Copyright (c) 2001 Waldo Bastian <bastian@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "main.h"

#include <QLayout>
//Added by qt3to4:
#include <QVBoxLayout>

#include <QtDBus/QtDBus>

#include <kapplication.h>
#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kdialog.h>
#include <KPluginFactory>
#include <KPluginLoader>

#include "mouse.h"
#include "windows.h"

K_PLUGIN_FACTORY_DECLARATION(KWinOptionsFactory)

class KFocusConfigStandalone : public KFocusConfig
{
    Q_OBJECT
public:
    KFocusConfigStandalone(QWidget* parent, const QVariantList &)
        : KFocusConfig(true, new KConfig("kwinrc"), KWinOptionsFactory::componentData(), parent)
    {}
};

class KMovingConfigStandalone : public KMovingConfig
{
    Q_OBJECT
public:
    KMovingConfigStandalone(QWidget* parent, const QVariantList &)
        : KMovingConfig(true, new KConfig("kwinrc"), KWinOptionsFactory::componentData(), parent)
    {}
};

class KAdvancedConfigStandalone : public KAdvancedConfig
{
    Q_OBJECT
public:
    KAdvancedConfigStandalone(QWidget* parent, const QVariantList &)
        : KAdvancedConfig(true, new KConfig("kwinrc"), KWinOptionsFactory::componentData(), parent)
    {}
};

KWinOptions::KWinOptions(QWidget *parent, const QVariantList &)
    : KCModule(KWinOptionsFactory::componentData(), parent)
{
    mConfig = new KConfig("kwinrc");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    tab = new KTabWidget(this);
    layout->addWidget(tab);

    mFocus = new KFocusConfig(false, mConfig, componentData(), this);
    mFocus->setObjectName(QLatin1String("KWin Focus Config"));
    tab->addTab(mFocus, i18n("&Focus"));
    connect(mFocus, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

    mTitleBarActions = new KTitleBarActionsConfig(false, mConfig, componentData(), this);
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions, i18n("&Titlebar Actions"));
    connect(mTitleBarActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

    mWindowActions = new KWindowActionsConfig(false, mConfig, componentData(), this);
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions, i18n("Window Actio&ns"));
    connect(mWindowActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

    mMoving = new KMovingConfig(false, mConfig, componentData(), this);
    mMoving->setObjectName(QLatin1String("KWin Moving"));
    tab->addTab(mMoving, i18n("&Moving"));
    connect(mMoving, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

    mAdvanced = new KAdvancedConfig(false, mConfig, componentData(), this);
    mAdvanced->setObjectName(QLatin1String("KWin Advanced"));
    tab->addTab(mAdvanced, i18n("Ad&vanced"));
    connect(mAdvanced, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

    KAboutData *about =
        new KAboutData(I18N_NOOP("kcmkwinoptions"), 0, ki18n("Window Behavior Configuration Module"),
                       0, KLocalizedString(), KAboutData::License_GPL,
                       ki18n("(c) 1997 - 2002 KWin and KControl Authors"));

    about->addAuthor(ki18n("Matthias Ettrich"), KLocalizedString(), "ettrich@kde.org");
    about->addAuthor(ki18n("Waldo Bastian"), KLocalizedString(), "bastian@kde.org");
    about->addAuthor(ki18n("Cristian Tibirna"), KLocalizedString(), "tibirna@kde.org");
    about->addAuthor(ki18n("Matthias Kalle Dalheimer"), KLocalizedString(), "kalle@kde.org");
    about->addAuthor(ki18n("Daniel Molkentin"), KLocalizedString(), "molkentin@kde.org");
    about->addAuthor(ki18n("Wynn Wilkes"), KLocalizedString(), "wynnw@caldera.com");
    about->addAuthor(ki18n("Pat Dowler"), KLocalizedString(), "dowler@pt1B1106.FSH.UVic.CA");
    about->addAuthor(ki18n("Bernd Wuebben"), KLocalizedString(), "wuebben@kde.org");
    about->addAuthor(ki18n("Matthias Hoelzer-Kluepfel"), KLocalizedString(), "hoelzer@kde.org");
    setAboutData(about);
}

KWinOptions::~KWinOptions()
{
    delete mConfig;
}

void KWinOptions::load()
{
    mConfig->reparseConfiguration();
    mFocus->load();
    mTitleBarActions->load();
    mWindowActions->load();
    mMoving->load();
    mAdvanced->load();
    emit KCModule::changed(false);
}


void KWinOptions::save()
{
    mFocus->save();
    mTitleBarActions->save();
    mWindowActions->save();
    mMoving->save();
    mAdvanced->save();

    emit KCModule::changed(false);
    // Send signal to kwin
    mConfig->sync();
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);


}


void KWinOptions::defaults()
{
    mFocus->defaults();
    mTitleBarActions->defaults();
    mWindowActions->defaults();
    mMoving->defaults();
    mAdvanced->defaults();
}

QString KWinOptions::quickHelp() const
{
    return i18n("<p><h1>Window Behavior</h1> Here you can customize the way windows behave when being"
                " moved, resized or clicked on. You can also specify a focus policy as well as a placement"
                " policy for new windows.</p>"
                " <p>Please note that this configuration will not take effect if you do not use"
                " KWin as your window manager. If you do use a different window manager, please refer to its documentation"
                " for how to customize window behavior.</p>");
}

void KWinOptions::moduleChanged(bool state)
{
    emit KCModule::changed(state);
}

KActionsOptions::KActionsOptions(QWidget *parent, const QVariantList &)
    : KCModule(KWinOptionsFactory::componentData(), parent)
{
    mConfig = new KConfig("kwinrc");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    tab = new KTabWidget(this);
    layout->addWidget(tab);

    mTitleBarActions = new KTitleBarActionsConfig(false, mConfig, componentData(), this);
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions, i18n("&Titlebar Actions"));
    connect(mTitleBarActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));

    mWindowActions = new KWindowActionsConfig(false, mConfig, componentData(), this);
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions, i18n("Window Actio&ns"));
    connect(mWindowActions, SIGNAL(changed(bool)), this, SLOT(moduleChanged(bool)));
}

KActionsOptions::~KActionsOptions()
{
    delete mConfig;
}

void KActionsOptions::load()
{
    mTitleBarActions->load();
    mWindowActions->load();
    emit KCModule::changed(false);
}


void KActionsOptions::save()
{
    mTitleBarActions->save();
    mWindowActions->save();

    emit KCModule::changed(false);
    // Send signal to kwin
    mConfig->sync();
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

}


void KActionsOptions::defaults()
{
    mTitleBarActions->defaults();
    mWindowActions->defaults();
}

void KActionsOptions::moduleChanged(bool state)
{
    emit KCModule::changed(state);
}

K_PLUGIN_FACTORY_DEFINITION(KWinOptionsFactory,
                            registerPlugin<KActionsOptions>("kwinactions");
                            registerPlugin<KFocusConfigStandalone>("kwinfocus");
                            registerPlugin<KMovingConfigStandalone>("kwinmoving");
                            registerPlugin<KAdvancedConfigStandalone>("kwinadvanced");
                            registerPlugin<KWinOptions>("kwinoptions");
                           )
K_EXPORT_PLUGIN(KWinOptionsFactory("kcmkwm"))

#include "main.moc"
#include "moc_main.cpp"

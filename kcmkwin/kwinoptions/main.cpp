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

#include <QtDBus>

#include <KLocalizedString>
#include <kconfig.h>
#include <kaboutdata.h>
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
        : KFocusConfig(true, new KConfig("kwinrc"), parent)
    {}
};

class KMovingConfigStandalone : public KMovingConfig
{
    Q_OBJECT
public:
    KMovingConfigStandalone(QWidget* parent, const QVariantList &)
        : KMovingConfig(true, parent)
    {}
};

class KAdvancedConfigStandalone : public KAdvancedConfig
{
    Q_OBJECT
public:
    KAdvancedConfigStandalone(QWidget* parent, const QVariantList &)
        : KAdvancedConfig(true, parent)
    {}
};

KWinOptions::KWinOptions(QWidget *parent, const QVariantList &)
    : KCModule(parent)
{
    mConfig = new KConfig("kwinrc");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(this);
    layout->addWidget(tab);

    mFocus = new KFocusConfig(false, mConfig, this);
    mFocus->setObjectName(QLatin1String("KWin Focus Config"));
    tab->addTab(mFocus, i18n("&Focus"));
    connect(mFocus, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));

    mTitleBarActions = new KTitleBarActionsConfig(false, mConfig, this);
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions, i18n("Titlebar A&ctions"));
    connect(mTitleBarActions, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));

    mWindowActions = new KWindowActionsConfig(false, mConfig, this);
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions, i18n("W&indow Actions"));
    connect(mWindowActions, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));

    mMoving = new KMovingConfig(false, this);
    mMoving->setObjectName(QLatin1String("KWin Moving"));
    tab->addTab(mMoving, i18n("Mo&vement"));
    connect(mMoving, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));
    connect(mMoving, qOverload<bool>(&KCModule::defaulted), this, qOverload<bool>(&KCModule::defaulted));

    mAdvanced = new KAdvancedConfig(false, this);
    mAdvanced->setObjectName(QLatin1String("KWin Advanced"));
    tab->addTab(mAdvanced, i18n("Adva&nced"));
    connect(mAdvanced, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));
    connect(mAdvanced, qOverload<bool>(&KCModule::defaulted), this, qOverload<bool>(&KCModule::defaulted));

    KAboutData *about =
        new KAboutData(QStringLiteral("kcmkwinoptions"), i18n("Window Behavior Configuration Module"),
                       QString(), QString(), KAboutLicense::GPL,
                       i18n("(c) 1997 - 2002 KWin and KControl Authors"));

    about->addAuthor(i18n("Matthias Ettrich"), QString(), "ettrich@kde.org");
    about->addAuthor(i18n("Waldo Bastian"), QString(), "bastian@kde.org");
    about->addAuthor(i18n("Cristian Tibirna"), QString(), "tibirna@kde.org");
    about->addAuthor(i18n("Matthias Kalle Dalheimer"), QString(), "kalle@kde.org");
    about->addAuthor(i18n("Daniel Molkentin"), QString(), "molkentin@kde.org");
    about->addAuthor(i18n("Wynn Wilkes"), QString(), "wynnw@caldera.com");
    about->addAuthor(i18n("Pat Dowler"), QString(), "dowler@pt1B1106.FSH.UVic.CA");
    about->addAuthor(i18n("Bernd Wuebben"), QString(), "wuebben@kde.org");
    about->addAuthor(i18n("Matthias Hoelzer-Kluepfel"), QString(), "hoelzer@kde.org");
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
    : KCModule(parent)
{
    mConfig = new KConfig("kwinrc");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(this);
    layout->addWidget(tab);

    mTitleBarActions = new KTitleBarActionsConfig(false, mConfig, this);
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions, i18n("&Titlebar Actions"));
    connect(mTitleBarActions, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));

    mWindowActions = new KWindowActionsConfig(false, mConfig, this);
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions, i18n("Window Actio&ns"));
    connect(mWindowActions, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));
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

    emit defaulted(true);
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

#include "main.moc"
#include "moc_main.cpp"

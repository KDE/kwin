/*
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "main.h"

#include <QLayout>
// Added by qt3to4:
#include <QVBoxLayout>

#include <QtDBus>

#include <KLocalizedString>
#include <KPluginFactory>
#include <kaboutdata.h>
#include <kconfig.h>

#include "kwinoptions_kdeglobals_settings.h"
#include "kwinoptions_settings.h"
#include "kwinoptionsdata.h"
#include "mouse.h"
#include "windows.h"

K_PLUGIN_CLASS_WITH_JSON(KWinOptions, "kcm_kwinoptions.json")

class KFocusConfigStandalone : public KFocusConfig
{
    Q_OBJECT
public:
    KFocusConfigStandalone(QWidget *parent, const QVariantList &)
        : KFocusConfig(true, nullptr, parent)
    {
        initialize(new KWinOptionsSettings(this));
    }
};

class KMovingConfigStandalone : public KMovingConfig
{
    Q_OBJECT
public:
    KMovingConfigStandalone(QWidget *parent, const QVariantList &)
        : KMovingConfig(true, nullptr, parent)
    {
        initialize(new KWinOptionsSettings(this));
    }
};

class KAdvancedConfigStandalone : public KAdvancedConfig
{
    Q_OBJECT
public:
    KAdvancedConfigStandalone(QWidget *parent, const QVariantList &)
        : KAdvancedConfig(true, nullptr, nullptr, parent)
    {
        initialize(new KWinOptionsSettings(this), new KWinOptionsKDEGlobalsSettings(this));
    }
};

KWinOptions::KWinOptions(QWidget *parent, const QVariantList &)
    : KCModule(parent)
{
    mSettings = new KWinOptionsSettings(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(this);
    layout->addWidget(tab);

    mFocus = new KFocusConfig(false, mSettings, this);
    mFocus->setObjectName(QLatin1String("KWin Focus Config"));
    tab->addTab(mFocus, i18n("&Focus"));
    connect(mFocus, qOverload<bool>(&KCModule::changed), this, qOverload<>(&KWinOptions::updateUnmanagedState));
    connect(this, &KCModule::defaultsIndicatorsVisibleChanged, mFocus, &KCModule::setDefaultsIndicatorsVisible);

    mTitleBarActions = new KTitleBarActionsConfig(false, mSettings, this);
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions, i18n("Titlebar A&ctions"));
    connect(mTitleBarActions, qOverload<bool>(&KCModule::changed), this, qOverload<>(&KWinOptions::updateUnmanagedState));
    connect(this, &KCModule::defaultsIndicatorsVisibleChanged, mTitleBarActions, &KCModule::setDefaultsIndicatorsVisible);

    mWindowActions = new KWindowActionsConfig(false, mSettings, this);
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions, i18n("W&indow Actions"));
    connect(mWindowActions, qOverload<bool>(&KCModule::changed), this, qOverload<>(&KWinOptions::updateUnmanagedState));
    connect(this, &KCModule::defaultsIndicatorsVisibleChanged, mWindowActions, &KCModule::setDefaultsIndicatorsVisible);

    mMoving = new KMovingConfig(false, mSettings, this);
    mMoving->setObjectName(QLatin1String("KWin Moving"));
    tab->addTab(mMoving, i18n("Mo&vement"));
    connect(mMoving, qOverload<bool>(&KCModule::changed), this, qOverload<>(&KWinOptions::updateUnmanagedState));
    connect(this, &KCModule::defaultsIndicatorsVisibleChanged, mMoving, &KCModule::setDefaultsIndicatorsVisible);

    mAdvanced = new KAdvancedConfig(false, mSettings, new KWinOptionsKDEGlobalsSettings(this), this);
    mAdvanced->setObjectName(QLatin1String("KWin Advanced"));
    tab->addTab(mAdvanced, i18n("Adva&nced"));
    connect(mAdvanced, qOverload<bool>(&KCModule::changed), this, qOverload<>(&KWinOptions::updateUnmanagedState));
    connect(this, &KCModule::defaultsIndicatorsVisibleChanged, mAdvanced, &KCModule::setDefaultsIndicatorsVisible);
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

void KWinOptions::load()
{
    KCModule::load();

    mTitleBarActions->load();
    mWindowActions->load();
    mMoving->load();
    mAdvanced->load();
    // mFocus is last because it may send unmanagedWidgetStateChanged
    // that need to have the final word
    mFocus->load();
}

void KWinOptions::save()
{
    KCModule::save();

    mFocus->save();
    mTitleBarActions->save();
    mWindowActions->save();
    mMoving->save();
    mAdvanced->save();

    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

void KWinOptions::defaults()
{
    KCModule::defaults();

    mTitleBarActions->defaults();
    mWindowActions->defaults();
    mMoving->defaults();
    mAdvanced->defaults();
    // mFocus is last because it may send unmanagedWidgetDefaulted
    // that need to have the final word
    mFocus->defaults();
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

void KWinOptions::updateUnmanagedState()
{
    bool isNeedSave = false;
    isNeedSave |= mFocus->isSaveNeeded();
    isNeedSave |= mTitleBarActions->isSaveNeeded();
    isNeedSave |= mWindowActions->isSaveNeeded();
    isNeedSave |= mMoving->isSaveNeeded();
    isNeedSave |= mAdvanced->isSaveNeeded();

    unmanagedWidgetChangeState(isNeedSave);

    bool isDefault = true;
    isDefault &= mFocus->isDefaults();
    isDefault &= mTitleBarActions->isDefaults();
    isDefault &= mWindowActions->isDefaults();
    isDefault &= mMoving->isDefaults();
    isDefault &= mAdvanced->isDefaults();

    unmanagedWidgetDefaultState(isDefault);
}

KActionsOptions::KActionsOptions(QWidget *parent, const QVariantList &)
    : KCModule(parent)
{
    mSettings = new KWinOptionsSettings(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(this);
    layout->addWidget(tab);

    mTitleBarActions = new KTitleBarActionsConfig(false, mSettings, this);
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions, i18n("&Titlebar Actions"));
    connect(mTitleBarActions, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));
    connect(mTitleBarActions, qOverload<bool>(&KCModule::defaulted), this, qOverload<bool>(&KCModule::defaulted));

    mWindowActions = new KWindowActionsConfig(false, mSettings, this);
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions, i18n("Window Actio&ns"));
    connect(mWindowActions, qOverload<bool>(&KCModule::changed), this, qOverload<bool>(&KCModule::changed));
    connect(mWindowActions, qOverload<bool>(&KCModule::defaulted), this, qOverload<bool>(&KCModule::defaulted));
}

void KActionsOptions::load()
{
    mTitleBarActions->load();
    mWindowActions->load();
}

void KActionsOptions::save()
{
    mTitleBarActions->save();
    mWindowActions->save();

    Q_EMIT KCModule::changed(false);
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
    Q_EMIT KCModule::changed(state);
}

#include "main.moc"

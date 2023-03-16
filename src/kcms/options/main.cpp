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

KWinOptions::KWinOptions(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
{
    mSettings = new KWinOptionsSettings(this);

    QVBoxLayout *layout = new QVBoxLayout(widget());
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(widget());
    layout->addWidget(tab);

    const auto connectKCM = [this](KCModule *mod) {
        connect(mod, &KCModule::needsSaveChanged, this, &KWinOptions::updateUnmanagedState);
        connect(this, &KCModule::defaultsIndicatorsVisibleChanged, mod, [mod, this]() {
            mod->setDefaultsIndicatorsVisible(defaultsIndicatorsVisible());
        });
    };

    mFocus = new KFocusConfig(false, mSettings, widget());
    mFocus->setObjectName(QLatin1String("KWin Focus Config"));
    tab->addTab(mFocus->widget(), i18n("&Focus"));
    connectKCM(mFocus);

    mTitleBarActions = new KTitleBarActionsConfig(false, mSettings, widget());
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions->widget(), i18n("Titlebar A&ctions"));
    connectKCM(mTitleBarActions);

    mWindowActions = new KWindowActionsConfig(false, mSettings, widget());
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions->widget(), i18n("W&indow Actions"));
    connectKCM(mWindowActions);

    mMoving = new KMovingConfig(false, mSettings, widget());
    mMoving->setObjectName(QLatin1String("KWin Moving"));
    tab->addTab(mMoving->widget(), i18n("Mo&vement"));
    connectKCM(mMoving);

    mAdvanced = new KAdvancedConfig(false, mSettings, new KWinOptionsKDEGlobalsSettings(this), widget());
    mAdvanced->setObjectName(QLatin1String("KWin Advanced"));
    tab->addTab(mAdvanced->widget(), i18n("Adva&nced"));
    connectKCM(mAdvanced);
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

KActionsOptions::KActionsOptions(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KCModule(parent, data, args)
{
    mSettings = new KWinOptionsSettings(this);

    QVBoxLayout *layout = new QVBoxLayout(widget());
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(widget());
    layout->addWidget(tab);

    mTitleBarActions = new KTitleBarActionsConfig(false, mSettings, widget());
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions->widget(), i18n("&Titlebar Actions"));
    connect(mTitleBarActions, &KCModule::needsSaveChanged, this, [this]() {
        setNeedsSave(mTitleBarActions->needsSave());
    });
    connect(mTitleBarActions, &KCModule::representsDefaultsChanged, this, [this]() {
        setRepresentsDefaults(mTitleBarActions->representsDefaults());
    });

    mWindowActions = new KWindowActionsConfig(false, mSettings, widget());
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions->widget(), i18n("Window Actio&ns"));
    connect(mWindowActions, &KCModule::needsSaveChanged, this, [this]() {
        setNeedsSave(mWindowActions->needsSave());
    });
    connect(mWindowActions, &KCModule::representsDefaultsChanged, this, [this]() {
        setRepresentsDefaults(mWindowActions->representsDefaults());
    });
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

    setNeedsSave(false);
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

void KActionsOptions::defaults()
{
    mTitleBarActions->defaults();
    mWindowActions->defaults();
}

void KActionsOptions::moduleChanged(bool state)
{
    setNeedsSave(state);
}

#include "main.moc"

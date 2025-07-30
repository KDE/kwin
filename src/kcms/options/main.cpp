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

KWinOptions::KWinOptions(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    mSettings = new KWinOptionsSettings(this);

    QVBoxLayout *layout = new QVBoxLayout(widget());
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(widget());
    tab->setDocumentMode(true);
    tab->tabBar()->setExpanding(true);
    layout->addWidget(tab);

    const auto connectKCM = [this](KCModule *mod) {
        connect(mod, &KCModule::needsSaveChanged, this, &KWinOptions::updateUnmanagedState);
        connect(this, &KCModule::defaultsIndicatorsVisibleChanged, mod, [mod, this]() {
            mod->setDefaultsIndicatorsVisible(defaultsIndicatorsVisible());
        });
    };

    mFocus = new KFocusConfig(mSettings, widget());
    mFocus->setObjectName(QLatin1String("KWin Focus Config"));
    tab->addTab(mFocus->widget(), i18n("&Focus"));
    connectKCM(mFocus);

    mTitleBarActions = new KTitleBarActionsConfig(mSettings, widget());
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions->widget(), i18n("Titlebar A&ctions"));
    connectKCM(mTitleBarActions);

    mWindowActions = new KWindowActionsConfig(mSettings, widget());
    mWindowActions->setObjectName(QLatin1String("KWin Window Actions"));
    tab->addTab(mWindowActions->widget(), i18n("W&indow Actions"));
    connectKCM(mWindowActions);

    mMoving = new KMovingConfig(mSettings, widget());
    mMoving->setObjectName(QLatin1String("KWin Moving"));
    tab->addTab(mMoving->widget(), i18n("Mo&vement"));
    connectKCM(mMoving);

    mAdvanced = new KAdvancedConfig(mSettings, new KWinOptionsKDEGlobalsSettings(this), widget());
    mAdvanced->setObjectName(QLatin1String("KWin Advanced"));
    tab->addTab(mAdvanced->widget(), i18n("Adva&nced"));
    connectKCM(mAdvanced);

    if (qEnvironmentVariableIntValue("KWIN_WAYLAND_SUPPORT_XX_PIP_V1")) {
        mPip = new KPipConfig(mSettings, widget());
        mPip->setObjectName(QLatin1String("KWin Picture-in-Picture"));
        tab->addTab(mPip->widget(), i18n("&Picture-in-picture"));
        connectKCM(mPip);
    }
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
    if (mPip) {
        mPip->load();
    }
}

void KWinOptions::save()
{
    KCModule::save();

    mFocus->save();
    mTitleBarActions->save();
    mWindowActions->save();
    mMoving->save();
    mAdvanced->save();
    if (mPip) {
        mPip->save();
    }

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
    if (mPip) {
        mPip->defaults();
    }
}

void KWinOptions::updateUnmanagedState()
{
    bool changed = false;
    changed |= mFocus->needsSave();
    changed |= mTitleBarActions->needsSave();
    changed |= mWindowActions->needsSave();
    changed |= mMoving->needsSave();
    changed |= mAdvanced->needsSave();
    changed |= mPip ? mPip->needsSave() : false;

    unmanagedWidgetChangeState(changed);

    bool isDefault = true;
    isDefault &= mFocus->representsDefaults();
    isDefault &= mTitleBarActions->representsDefaults();
    isDefault &= mWindowActions->representsDefaults();
    isDefault &= mMoving->representsDefaults();
    isDefault &= mAdvanced->representsDefaults();
    isDefault &= mPip ? mPip->representsDefaults() : true;

    unmanagedWidgetDefaultState(isDefault);
}

KActionsOptions::KActionsOptions(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    mSettings = new KWinOptionsSettings(this);

    QVBoxLayout *layout = new QVBoxLayout(widget());
    layout->setContentsMargins(0, 0, 0, 0);
    tab = new QTabWidget(widget());
    layout->addWidget(tab);

    mTitleBarActions = new KTitleBarActionsConfig(mSettings, widget());
    mTitleBarActions->setObjectName(QLatin1String("KWin TitleBar Actions"));
    tab->addTab(mTitleBarActions->widget(), i18n("&Titlebar Actions"));
    connect(mTitleBarActions, &KCModule::needsSaveChanged, this, [this]() {
        setNeedsSave(mTitleBarActions->needsSave());
    });
    connect(mTitleBarActions, &KCModule::representsDefaultsChanged, this, [this]() {
        setRepresentsDefaults(mTitleBarActions->representsDefaults());
    });

    mWindowActions = new KWindowActionsConfig(mSettings, widget());
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

#include "moc_main.cpp"

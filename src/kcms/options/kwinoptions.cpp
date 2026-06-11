/*
    SPDX-FileCopyrightText: 2026 Tobias Ozór <tobiasozor@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinoptions.h"
#include "kwinoptionsdata.h"

#include <KPluginFactory>
#include <QDBusConnection>
#include <QDBusMessage>
#include <qqml.h>

K_PLUGIN_CLASS_WITH_JSON(KWinOptions, "kcm_kwinoptions.json")

KWinOptions::KWinOptions(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_kwinSettings(new KWinOptionsSettings(this))
{
    qmlRegisterAnonymousType<KWinOptionsSettings>("org.kde.plasma.kcm.kwinoptions", 0);

    qmlRegisterUncreatableType<KWinOptionsSettings>("org.kde.plasma.kcm.kwinoptions",
                                                    1,
                                                    0,
                                                    "KWinOptionsSettings",
                                                    QStringLiteral("Registered for enum access only"));

    setButtons(Apply | Default | Help);
}

void KWinOptions::save()
{
    KQuickManagedConfigModule::save();

    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

bool KWinOptions::isPiPEnabled() const
{
    return qEnvironmentVariableIsSet("KWIN_WAYLAND_SUPPORT_XX_PIP_V1");
}

KWinOptionsSettings *KWinOptions::kwinSettings() const
{
    return m_kwinSettings;
}

#include "kwinoptions.moc"
#include "moc_kwinoptions.cpp"

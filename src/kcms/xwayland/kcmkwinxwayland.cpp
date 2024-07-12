/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kcmkwinxwayland.h"

#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QKeySequence>

#include <kwinxwaylanddata.h>

K_PLUGIN_FACTORY_WITH_JSON(KcmXwaylandFactory, "kcm_kwinxwayland.json", registerPlugin<KcmXwayland>(); registerPlugin<KWinXwaylandData>();)

KcmXwayland::KcmXwayland(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_data(new KWinXwaylandData(this))
    , m_settings(new KWinXwaylandSettings(m_data))
{
    registerSettings(m_settings);
    qmlRegisterAnonymousType<KWinXwaylandSettings>("org.kde.kwin.kwinxwaylandsettings", 1);
}

void KcmXwayland::logout() const
{
    auto method = QDBusMessage::createMethodCall(QStringLiteral("org.kde.LogoutPrompt"),
                                                 QStringLiteral("/LogoutPrompt"),
                                                 QStringLiteral("org.kde.LogoutPrompt"),
                                                 QStringLiteral("promptLogout"));
    QDBusConnection::sessionBus().asyncCall(method);
}

void KcmXwayland::save()
{
    bool modifiedXwaylandEis = m_settings->xwaylandEisNoPromptItem()->isSaveNeeded();
    KQuickManagedConfigModule::save();
    if (modifiedXwaylandEis) {
        Q_EMIT showLogoutMessage();
    }
}

KcmXwayland::~KcmXwayland() = default;

#include "kcmkwinxwayland.moc"

#include "moc_kcmkwinxwayland.cpp"

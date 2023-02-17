/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kcmkwinxwayland.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QKeySequence>

#include <kwinxwaylanddata.h>

K_PLUGIN_FACTORY_WITH_JSON(KcmXwaylandFactory, "kcm_kwinxwayland.json", registerPlugin<KcmXwayland>(); registerPlugin<KWinXwaylandData>();)

KcmXwayland::KcmXwayland(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
    : KQuickAddons::ManagedConfigModule(parent, metaData, args)
    , m_data(new KWinXwaylandData(this))
    , m_settings(new KWinXwaylandSettings(m_data))
{
    registerSettings(m_settings);
    qmlRegisterAnonymousType<KWinXwaylandSettings>("org.kde.kwin.kwinxwaylandsettings", 1);
}

KcmXwayland::~KcmXwayland() = default;

#include "kcmkwinxwayland.moc"

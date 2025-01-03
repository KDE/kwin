/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinscriptsdata.h"

#include "config-kwin.h"

#include <KConfigGroup>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KPackage/PackageStructure>
#include <KPluginFactory>

KWinScriptsData::KWinScriptsData(QObject *parent)
    : KCModuleData(parent)
    , m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
{
}

QList<KPluginMetaData> KWinScriptsData::pluginMetaDataList() const
{
    return KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Script"), KWIN_DATADIR + QStringLiteral("/scripts/"))
        + KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Script"), QStringLiteral("kwin/scripts/"));
}

bool KWinScriptsData::isDefaults() const
{
    QList<KPluginMetaData> plugins = pluginMetaDataList();
    KConfigGroup cfgGroup(m_kwinConfig, QStringLiteral("Plugins"));
    for (auto &plugin : plugins) {
        if (cfgGroup.readEntry(plugin.pluginId() + QLatin1String("Enabled"), plugin.isEnabledByDefault()) != plugin.isEnabledByDefault()) {
            return false;
        }
    }

    return true;
}

#include "moc_kwinscriptsdata.cpp"

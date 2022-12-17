/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinscriptsdata.h"

#include <KConfigGroup>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KPackage/PackageStructure>
#include <KPluginFactory>

KWinScriptsData::KWinScriptsData(QObject *parent, const QVariantList &args)
    : KCModuleData(parent, args)
    , m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
{
}

QVector<KPluginMetaData> KWinScriptsData::pluginMetaDataList() const
{
    auto filter = [](const KPluginMetaData &md) {
        return md.isValid() && !md.rawData().value("X-KWin-Exclude-Listing").toBool();
    };

    const QString scriptFolder = QStringLiteral("kwin/scripts/");
    return KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Script"), scriptFolder, filter).toVector();
}

bool KWinScriptsData::isDefaults() const
{
    QVector<KPluginMetaData> plugins = pluginMetaDataList();
    KConfigGroup cfgGroup(m_kwinConfig, "Plugins");
    for (auto &plugin : plugins) {
        if (cfgGroup.readEntry(plugin.pluginId() + QLatin1String("Enabled"), plugin.isEnabledByDefault()) != plugin.isEnabledByDefault()) {
            return false;
        }
    }

    return true;
}

/*
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinscriptsdata.h"

#include <KPluginFactory>
#include <KPackage/PackageLoader>
#include <KPackage/Package>
#include <KPackage/PackageStructure>
#include "kpluginselector.h"


KWinScriptsData::KWinScriptsData(QObject *parent, const QVariantList &args)
    : KCModuleData(parent, args)
    , m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
{
}

QList<KPluginInfo> KWinScriptsData::pluginInfoList() const
{
    auto filter =  [](const KPluginMetaData &md) {
        return md.isValid() && !md.rawData().value("X-KWin-Exclude-Listing").toBool();
    };

    const QString scriptFolder = QStringLiteral("kwin/scripts/");
    const auto scripts = KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Script"), scriptFolder, filter);

    return KPluginInfo::fromMetaData(scripts.toVector());
}

bool KWinScriptsData::isDefaults() const
{
    QList<KPluginInfo> scriptinfos = pluginInfoList();
    KConfigGroup cfgGroup(m_kwinConfig, "Plugins");
    for (auto &script : scriptinfos) {
        script.load(cfgGroup);
        if (script.isPluginEnabled() != script.isPluginEnabledByDefault()) {
            return false;
        }
    }

    return true;
}

#include "kwinscriptsdata.moc"

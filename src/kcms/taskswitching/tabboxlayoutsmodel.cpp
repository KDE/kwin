/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QCollator>
#include <QDir>
#include <QStandardItemModel>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KSharedConfig>
#include <qnamespace.h>

#include "tabboxlayoutsmodel.h"

namespace KWin
{

TabBoxLayoutsModel::TabBoxLayoutsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    load();
}

void TabBoxLayoutsModel::load()
{
    beginResetModel();

    m_layouts.clear();

    // Check Plasma LnFs
    QList<KPackage::Package> lnfPackages;
    QStringList lnfPaths;

    for (const QString &path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        QDir dir(path + QLatin1String("/plasma/look-and-feel"));
        lnfPaths << dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    for (const QString &path : lnfPaths) {
        KPackage::Package pkg = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/LookAndFeel"));
        pkg.setPath(path);
        pkg.setFallbackPackage(KPackage::Package());
        if (!pkg.filePath("defaults").isEmpty()) {
            KSharedConfigPtr conf = KSharedConfig::openConfig(pkg.filePath("defaults"));
            KConfigGroup cg = KConfigGroup(conf, QStringLiteral("kwinrc"));
            cg = KConfigGroup(&cg, QStringLiteral("WindowSwitcher"));
            if (!cg.readEntry("LayoutName", QString()).isEmpty()) {
                lnfPackages << pkg;
            }
        }
    }

    for (const auto &package : lnfPackages) {
        const auto &metaData = package.metadata();
        const QString switcherFile = package.filePath("windowswitcher", QStringLiteral("WindowSwitcher.qml"));
        if (switcherFile.isEmpty()) {
            // Skip lnfs that don't actually ship a switcher
            continue;
        }

        m_layouts << LayoutData{metaData.name(), metaData.description(), metaData.pluginId(), switcherFile};
    }

    // Check packages
    const QList<KPluginMetaData> offers = KPackage::PackageLoader::self()->listPackages(QStringLiteral("KWin/WindowSwitcher"));
    for (const auto &offer : offers) {
        const QString pluginName = offer.pluginId();
        const QString scriptFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                          u"kwin/tabbox/" + pluginName + QLatin1String("/contents/ui/main.qml"));
        if (scriptFile.isEmpty()) {
            qWarning() << "scriptfile is null" << pluginName;
            continue;
        }

        m_layouts << LayoutData{offer.name(), offer.description(), pluginName, scriptFile};
    }

    // Sort
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_layouts.begin(), m_layouts.end(), [&collator](const LayoutData &a, const LayoutData &b) {
        return collator.compare(a.name, b.name) < 0;
    });

    endResetModel();
}

QHash<int, QByteArray> TabBoxLayoutsModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {PluginIdRole, "pluginId"},
        {PathRole, "path"},
    };
}

int TabBoxLayoutsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_layouts.count();
}

QVariant TabBoxLayoutsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto &data = m_layouts.at(index.row());
    switch (role) {
    case NameRole:
        return data.name;
    case DescriptionRole:
        return data.description;
    case PluginIdRole:
        return data.pluginId;
    case PathRole:
        return data.path;
    default:
        return {};
    }
}

TabBoxLayoutsProxyModel::TabBoxLayoutsProxyModel(TabBoxLayoutsModel *layouts, QObject *parent)
    : QConcatenateTablesProxyModel(parent)
{
    auto none = new QStandardItem(i18nc("@item:inlistbox", "None"));
    none->setData(QString(), TabBoxLayoutsModel::DescriptionRole);
    none->setData(QString(), TabBoxLayoutsModel::PluginIdRole);
    none->setData(QString(), TabBoxLayoutsModel::PathRole);

    auto noneModel = new QStandardItemModel(this);
    noneModel->setItemRoleNames(layouts->roleNames());
    noneModel->appendRow(none);

    addSourceModel(noneModel);
    addSourceModel(layouts);
}

} // namespace KWin

#include "moc_tabboxlayoutsmodel.cpp"

/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "decorationmodel.h"
// KDecoration2
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationSettings>
#include <KDecoration2/DecorationThemeProvider>
// KDE
#include <KPluginFactory>
#include <KPluginMetaData>
// Qt
#include <QDebug>

namespace KDecoration2
{

namespace Configuration
{
static const QString s_pluginName = QStringLiteral("org.kde.kdecoration2");

DecorationsModel::DecorationsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

DecorationsModel::~DecorationsModel() = default;

int DecorationsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_plugins.size();
}

QVariant DecorationsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= int(m_plugins.size())) {
        return QVariant();
    }
    const KDecoration2::DecorationThemeMetaData &d = m_plugins.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return d.visibleName();
    case PluginNameRole:
        return d.pluginId();
    case ThemeNameRole:
        return d.themeName();
    case ConfigurationRole:
        return d.hasConfiguration();
    case RecommendedBorderSizeRole:
        return Utils::borderSizeToString(d.borderSize());
    }
    return QVariant();
}

QHash<int, QByteArray> DecorationsModel::roleNames() const
{
    QHash<int, QByteArray> roles({{Qt::DisplayRole, QByteArrayLiteral("display")},
                                  {PluginNameRole, QByteArrayLiteral("plugin")},
                                  {ThemeNameRole, QByteArrayLiteral("theme")},
                                  {ConfigurationRole, QByteArrayLiteral("configureable")},
                                  {RecommendedBorderSizeRole, QByteArrayLiteral("recommendedbordersize")}});
    return roles;
}

static bool isThemeEngine(const QVariantMap &decoSettingsMap)
{
    auto it = decoSettingsMap.find(QStringLiteral("themes"));
    if (it == decoSettingsMap.end()) {
        return false;
    }
    return it.value().toBool();
}

static bool isConfigureable(const QVariantMap &decoSettingsMap)
{
    auto it = decoSettingsMap.find(QStringLiteral("kcmodule"));
    if (it == decoSettingsMap.end()) {
        return false;
    }
    return it.value().toBool();
}

static KDecoration2::BorderSize recommendedBorderSize(const QVariantMap &decoSettingsMap)
{
    auto it = decoSettingsMap.find(QStringLiteral("recommendedBorderSize"));
    if (it == decoSettingsMap.end()) {
        return KDecoration2::BorderSize::Normal;
    }
    return Utils::stringToBorderSize(it.value().toString());
}

static QString themeListKeyword(const QVariantMap &decoSettingsMap)
{
    auto it = decoSettingsMap.find(QStringLiteral("themeListKeyword"));
    if (it == decoSettingsMap.end()) {
        return QString();
    }
    return it.value().toString();
}

static QString findKNewStuff(const QVariantMap &decoSettingsMap)
{
    auto it = decoSettingsMap.find(QStringLiteral("KNewStuff"));
    if (it == decoSettingsMap.end()) {
        return QString();
    }
    return it.value().toString();
}

void DecorationsModel::init()
{
    beginResetModel();
    m_plugins.clear();
    const auto plugins = KPluginMetaData::findPlugins(s_pluginName);
    for (const auto &info : plugins) {
        std::unique_ptr<KDecoration2::DecorationThemeProvider> themeFinder(
            KPluginFactory::instantiatePlugin<KDecoration2::DecorationThemeProvider>(info).plugin);
        KDecoration2::DecorationThemeMetaData data;
        const auto decoSettingsMap = info.rawData().value("org.kde.kdecoration2").toObject().toVariantMap();
        if (themeFinder) {
            const QString &kns = findKNewStuff(decoSettingsMap);
            if (!kns.isEmpty() && !m_knsProviders.contains(kns)) {
                m_knsProviders.append(kns);
            }
            if (isThemeEngine(decoSettingsMap)) {
                const QString keyword = themeListKeyword(decoSettingsMap);
                if (keyword.isNull()) {
                    // We cannot list the themes
                    continue;
                }
                const auto themesList = themeFinder->themes();
                for (const KDecoration2::DecorationThemeMetaData &data : themesList) {
                    m_plugins.emplace_back(data);
                }

                // it's a theme engine, we don't want to show this entry
                continue;
            }
        }
        data.setHasConfiguration(isConfigureable(decoSettingsMap));
        data.setBorderSize(recommendedBorderSize(decoSettingsMap));
        data.setVisibleName(info.name().isEmpty() ? info.pluginId() : info.name());
        data.setPluginId(info.pluginId());
        data.setThemeName(data.visibleName());

        m_plugins.emplace_back(std::move(data));
    }
    endResetModel();
}

QModelIndex DecorationsModel::findDecoration(const QString &pluginName, const QString &themeName) const
{
    auto it = std::find_if(m_plugins.cbegin(), m_plugins.cend(), [pluginName, themeName](const KDecoration2::DecorationThemeMetaData &d) {
        return d.pluginId() == pluginName && d.themeName() == themeName;
    });
    if (it == m_plugins.cend()) {
        return QModelIndex();
    }
    const auto distance = std::distance(m_plugins.cbegin(), it);
    return createIndex(distance, 0);
}

}
}

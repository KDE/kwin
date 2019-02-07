/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "decorationmodel.h"
// KDecoration2
#include <KDecoration2/DecorationSettings>
#include <KDecoration2/Decoration>
// KDE
#include <KPluginInfo>
#include <KPluginLoader>
#include <KPluginFactory>
#include <KPluginTrader>
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
    const Data &d = m_plugins.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return d.visibleName;
    case PluginNameRole:
        return d.pluginName;
    case ThemeNameRole:
        return d.themeName;
    case ConfigurationRole:
        return d.configuration;
    }
    return QVariant();
}

QHash< int, QByteArray > DecorationsModel::roleNames() const
{
    QHash<int, QByteArray> roles({
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {PluginNameRole, QByteArrayLiteral("plugin")},
        {ThemeNameRole, QByteArrayLiteral("theme")},
        {ConfigurationRole, QByteArrayLiteral("configureable")}
    });
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
    const auto plugins = KPluginTrader::self()->query(s_pluginName, s_pluginName);
    for (const auto &info : plugins) {
        KPluginLoader loader(info.libraryPath());
        KPluginFactory *factory = loader.factory();
        if (!factory) {
            continue;
        }
        auto metadata = loader.metaData().value(QStringLiteral("MetaData")).toObject().value(s_pluginName);
        bool config = false;
        if (!metadata.isUndefined()) {
            const auto decoSettingsMap = metadata.toObject().toVariantMap();
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
                QScopedPointer<QObject> themeFinder(factory->create<QObject>(keyword));
                if (themeFinder.isNull()) {
                    continue;
                }
                QVariant themes = themeFinder->property("themes");
                if (!themes.isValid()) {
                    continue;
                }
                const auto themesMap = themes.toMap();
                for (auto it = themesMap.begin(); it != themesMap.end(); ++it) {
                    Data d;
                    d.pluginName = info.pluginName();
                    d.themeName = it.value().toString();
                    d.visibleName = it.key();
                    QMetaObject::invokeMethod(themeFinder.data(), "hasConfiguration",
                                              Q_RETURN_ARG(bool, d.configuration),
                                              Q_ARG(QString, d.themeName));
                    m_plugins.emplace_back(std::move(d));
                }

                // it's a theme engine, we don't want to show this entry
                continue;
            }
            config = isConfigureable(decoSettingsMap);
        }
        Data data;
        data.pluginName = info.pluginName();
        data.visibleName = info.name().isEmpty() ? info.pluginName() : info.name();
        data.themeName = data.visibleName;
        data.configuration = config;

        m_plugins.emplace_back(std::move(data));
    }
    endResetModel();
}

QModelIndex DecorationsModel::findDecoration(const QString &pluginName, const QString &themeName) const
{
    auto it = std::find_if(m_plugins.cbegin(), m_plugins.cend(),
        [pluginName, themeName](const Data &d) {
            return d.pluginName == pluginName && d.themeName == themeName;
        }
    );
    if (it == m_plugins.cend()) {
        return QModelIndex();
    }
    const auto distance = std::distance(m_plugins.cbegin(), it);
    return createIndex(distance, 0);
}

}
}

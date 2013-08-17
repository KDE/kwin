/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
*                                                                        *
* This program is free software; you can redistribute it and/or modify   *
* it under the terms of the GNU General Public License as published by   *
* the Free Software Foundation; either version 2 of the License, or      *
* (at your option) any later version.                                    *
*                                                                        *
* This program is distributed in the hope that it will be useful,        *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
**************************************************************************/

#include "model.h"
#include "effectconfig.h"
#include "compositing.h"

#include <KDE/KPluginInfo>
#include <KDE/KService>
#include <KDE/KServiceTypeTrader>
#include <KDE/KSharedConfig>
#include <KDE/KCModuleProxy>

#include <QAbstractItemModel>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusMessage>
#include <QHash>
#include <QVariant>
#include <QList>
#include <QString>
#include <QQmlEngine>
#include <QtQml>
#include <QDebug>

namespace KWin {
namespace Compositing {

EffectModel::EffectModel(QObject *parent)
    : QAbstractItemModel(parent) {

    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "NameRole";
    roleNames[DescriptionRole] = "DescriptionRole";
    roleNames[AuthorNameRole] = "AuthorNameRole";
    roleNames[AuthorEmailRole] = "AuthorEmailRole";
    roleNames[LicenseRole] = "LicenseRole";
    roleNames[VersionRole] = "VersionRole";
    roleNames[CategoryRole] = "CategoryRole";
    roleNames[ServiceNameRole] = "ServiceNameRole";
    roleNames[EffectStatusRole] = "EffectStatusRole";
    setRoleNames(roleNames);
    loadEffects();
}

QModelIndex EffectModel::index(int row, int column, const QModelIndex &parent) const {
if (parent.isValid() || column > 0 || column < 0 || row < 0 || row >= m_effectsList.count()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex EffectModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)

    return QModelIndex();
}

int EffectModel::columnCount(const QModelIndex &parent) const {
    return 1;
}

int EffectModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_effectsList.count();
}

QVariant EffectModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }

    EffectData currentEffect = m_effectsList.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return m_effectsList.at(index.row()).name;
        case DescriptionRole:
            return m_effectsList.at(index.row()).description;
        case AuthorNameRole:
            return m_effectsList.at(index.row()).authorName;
        case AuthorEmailRole:
            return m_effectsList.at(index.row()).authorEmail;
        case LicenseRole:
            return m_effectsList.at(index.row()).license;
        case VersionRole:
            return m_effectsList.at(index.row()).version;
        case CategoryRole:
            return m_effectsList.at(index.row()).category;
        case ServiceNameRole:
            return m_effectsList.at(index.row()).serviceName;
        case EffectStatusRole:
            return m_effectsList.at(index.row()).effectStatus;
        default:
            return QVariant();
    }
}

bool EffectModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid())
        return QAbstractItemModel::setData(index, value, role);

    if (role == EffectModel::EffectStatusRole) {
        m_effectsList[index.row()].effectStatus = value.toBool();
        emit dataChanged(index, index);
        return true;
    }

    return QAbstractItemModel::setData(index, value, role);
}

void EffectModel::loadEffects() {
    EffectData effect;
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");
    QDBusInterface interface(QStringLiteral("org.kde.kwin"), QStringLiteral("/Effects"));

    beginResetModel();
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    for(KService::Ptr service : offers) {
        KPluginInfo plugin(service);
        effect.name = plugin.name();
        effect.description = plugin.comment();
        effect.authorName = plugin.author();
        effect.authorEmail = plugin.email();
        effect.license = plugin.license();
        effect.version = plugin.version();
        effect.category = plugin.category();
        effect.serviceName = serviceName(effect.name);
        effect.effectStatus = kwinConfig.readEntry(effect.serviceName + "Enabled", false);

        if (effect.effectStatus) {
            interface.asyncCall("loadEffect", effect.serviceName);
        } else {
            interface.asyncCall("unloadEffect", effect.serviceName);
        }

        m_effectsList << effect;
    }

    qSort(m_effectsList.begin(), m_effectsList.end(), [](const EffectData &a, const EffectData &b) {
        return a.category < b.category;
    });

    endResetModel();
}

QString EffectModel::serviceName(const QString &effectName) {
    //The effect name is something like "Show Fps" and
    //we want something like "showfps"
    return "kwin4_effect_" + effectName.toLower().remove(" ");
}

bool EffectModel::effectListContains(const QString &effectFilter, int source_row) {
    EffectData effect;
    effect = m_effectsList.at(source_row);

    return effect.name.contains(effectFilter, Qt::CaseInsensitive);

}

QString EffectModel::findImage(const QString &imagePath, int size) {
    const QString relativePath("icons/oxygen/" + QString::number(size) + 'x' + QString::number(size) + '/' + imagePath);
    const QString fullImagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, relativePath, QStandardPaths::LocateFile);
    return fullImagePath;
}

void EffectModel::reload() {
    m_effectsList.clear();
    loadEffects();
}

void EffectModel::effectStatus(const QModelIndex &index, bool effectState) {
    setData(index, effectState, EffectStatusRole);
}

void EffectModel::syncConfig() {
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    for (auto it = m_effectsList.begin(); it != m_effectsList.end(); it++) {
        EffectData effect = *(it);
        bool effectConfigStatus = kwinConfig.readEntry(effect.serviceName + "Enabled", false);

        if (effect.effectStatus) {
            kwinConfig.writeEntry(effect.serviceName + "Enabled", effect.effectStatus);
        } else if (effect.effectStatus != effectConfigStatus) {
            kwinConfig.writeEntry(effect.serviceName + "Enabled", effect.effectStatus);
        }
    }

    kwinConfig.sync();
}

EffectFilterModel::EffectFilterModel(QObject *parent)
    :QSortFilterProxyModel(parent),
    m_effectModel(0)
{
}

EffectModel *EffectFilterModel::effectModel() const {
    return m_effectModel;
}

const QString &EffectFilterModel::filter() const {
    return m_filter;
}

void EffectFilterModel::setEffectModel(EffectModel *effectModel) {
    if (effectModel == m_effectModel) {
        return;
    }

    m_effectModel = effectModel;
    setSourceModel(m_effectModel);
    emit effectModelChanged();
}

void EffectFilterModel::setFilter(const QString &filter) {
    if (filter == m_filter) {
        return;
    }

    m_filter = filter;
    emit filterChanged();
    invalidateFilter();
}

bool EffectFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    if (!m_effectModel) {
        return false;
    }

    if (m_filter.isEmpty()) {
        return true;
    }

    QModelIndex index = m_effectModel->index(source_row, 0, source_parent);
    if (!index.isValid()) {
        return false;
    }

    QVariant data = index.data();
    if (!data.isValid()) {
        //An invalid QVariant is valid data
        return true;
    }

    if (m_effectModel->effectListContains(m_filter, source_row)) {
        return true;
    }

    return false;
}

EffectView::EffectView(QWindow *parent)
    : QQuickView(parent)
{
    qmlRegisterType<EffectModel>("org.kde.kwin.kwincompositing", 1, 0, "EffectModel");
    qmlRegisterType<EffectConfig>("org.kde.kwin.kwincompositing", 1, 0, "EffectConfig");
    qmlRegisterType<EffectFilterModel>("org.kde.kwin.kwincompositing", 1, 0, "EffectFilterModel");
    qmlRegisterType<Compositing>("org.kde.kwin.kwincompositing", 1, 0, "Compositing");
    init();
}

void EffectView::init() {
    QString mainFile = QStandardPaths::locate(QStandardPaths::DataLocation, "qml/main.qml", QStandardPaths::LocateFile);
    setResizeMode(QQuickView::SizeRootObjectToView);
    setSource(QUrl(mainFile));
}

}//end namespace Compositing
}//end namespace KWin

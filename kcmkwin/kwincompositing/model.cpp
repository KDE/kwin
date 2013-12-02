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
#include <QQuickItem>
#include <QDebug>

namespace KWin {
namespace Compositing {

EffectModel::EffectModel(QObject *parent)
    : QAbstractItemModel(parent) {

    loadEffects();
}

QHash< int, QByteArray > EffectModel::roleNames() const
{
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
    return roleNames;
}

QModelIndex EffectModel::index(int row, int column, const QModelIndex &parent) const
{
if (parent.isValid() || column > 0 || column < 0 || row < 0 || row >= m_effectsList.count()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex EffectModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)

    return QModelIndex();
}

int EffectModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int EffectModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_effectsList.count();
}

QVariant EffectModel::data(const QModelIndex &index, int role) const
{
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

bool EffectModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return QAbstractItemModel::setData(index, value, role);

    if (role == EffectModel::EffectStatusRole) {
        m_effectsList[index.row()].effectStatus = value.toBool();

        const QString effectServiceName = m_effectsList[index.row()].serviceName;
        if (effectServiceName == "kwin4_effect_slide") {
            handleDesktopSwitching(index.row());
        } else if (effectServiceName == "kwin4_effect_fadedesktop") {
            handleDesktopSwitching(index.row());
        } else if (effectServiceName == "kwin4_effect_cubeslide") {
            handleDesktopSwitching(index.row());
        }
        emit dataChanged(index, index);
        return true;
    } else if (role == EffectModel::WindowManagementRole) {
        bool enabled = value.toBool();
        handleWindowManagement(index.row(), enabled);
        emit dataChanged(index, index);
        return true;
    }

    return QAbstractItemModel::setData(index, value, role);
}

void EffectModel::loadEffects()
{
    EffectData effect;
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    beginResetModel();
    m_effectsChanged.clear();
    m_effectsList.clear();
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    for(KService::Ptr service : offers) {
        const QString effectPluginPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kde5/services/"+ service->entryPath(), QStandardPaths::LocateFile);
        KPluginInfo plugin(effectPluginPath);

        effect.name = plugin.name();
        effect.description = plugin.comment();
        effect.authorName = plugin.author();
        effect.authorEmail = plugin.email();
        effect.license = plugin.license();
        effect.version = plugin.version();
        effect.category = plugin.category();
        effect.serviceName = plugin.pluginName();
        effect.effectStatus = kwinConfig.readEntry(effect.serviceName + "Enabled", plugin.isPluginEnabledByDefault());
        effect.enabledByDefault = plugin.isPluginEnabledByDefault();

        m_effectsList << effect;
    }

    qSort(m_effectsList.begin(), m_effectsList.end(), [](const EffectData &a, const EffectData &b) {
        return a.category < b.category;
    });

    m_effectsChanged = m_effectsList;
    endResetModel();
}

void EffectModel::handleDesktopSwitching(int row)
{
    //Q: Why do we need the handleDesktopSwitching?
    //A: Because of the setData, when we enable the effect
    //and then we scroll, our model is being updated,
    //so the setData is being called again, and as a result
    //of that we have multiple effects enabled for the desktop switching.
    const QString currentEffect = m_effectsList[row].serviceName;
    for (int it = 0; it < m_effectsList.size(); it++) {
        EffectData effect = m_effectsList.at(it);

        if (effect.serviceName == "kwin4_effect_slide" && currentEffect != effect.serviceName && effect.effectStatus) {
            m_effectsList[it].effectStatus = !m_effectsList[it].effectStatus;
        } else if (effect.serviceName == "kwin4_effect_cubeslide" && currentEffect != effect.serviceName && effect.effectStatus) {
            m_effectsList[it].effectStatus = !m_effectsList[it].effectStatus;
        } else if (effect.serviceName == "kwin4_effect_fadedesktop" && currentEffect != effect.serviceName && effect.effectStatus) {
            m_effectsList[it].effectStatus = !m_effectsList[it].effectStatus;
        }
    }
}

void EffectModel::handleWindowManagement(int row, bool enabled)
{
    //Make sure that our row is valid
    if (m_effectsList.size() > 0 && row <= m_effectsList.size())
        m_effectsList[row].effectStatus = enabled;
}

int EffectModel::findRowByServiceName(const QString &serviceName)
{
    for (int it = 0; it < m_effectsList.size(); it++) {
        if (m_effectsList.at(it).serviceName == serviceName) {
            return it;
        }
    }
    return -1;
}

void EffectModel::syncEffectsToKWin()
{
    QDBusInterface interface(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"));
    for (int it = 0; it < m_effectsList.size(); it++) {
        if (m_effectsList.at(it).effectStatus != m_effectsChanged.at(it).effectStatus) {
            if (m_effectsList.at(it).effectStatus) {
                interface.asyncCall("loadEffect", m_effectsList.at(it).serviceName);
            } else {
                interface.asyncCall("unloadEffect", m_effectsList.at(it).serviceName);
            }
        }
    }

    m_effectsChanged = m_effectsList;
}

void EffectModel::updateEffectStatus(const QModelIndex &rowIndex, bool effectState)
{
    setData(rowIndex, effectState, EffectModel::EffectStatusRole);
}

void EffectModel::syncConfig()
{
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
    syncEffectsToKWin();
}

void EffectModel::enableWidnowManagement(bool enabled)
{
    //Make sure that our effects are being installed properly
    if (m_effectsList.size() <= 0)
        return;

    int desktopGridRow = findRowByServiceName("kwin4_effect_desktopgrid");
    const QModelIndex desktopGridIndex = createIndex(desktopGridRow, 0);
    setData(desktopGridIndex, enabled, EffectModel::WindowManagementRole);

    int dialogParentRow = findRowByServiceName("kwin4_effect_dialogparent");
    const QModelIndex dialogParentIndex = createIndex(dialogParentRow, 0);
    setData(dialogParentIndex, enabled, EffectModel::WindowManagementRole);

    int presentWindowsRow = findRowByServiceName("kwin4_effect_presentwindows");
    const QModelIndex presentWindowsIndex = createIndex(presentWindowsRow, 0);
    setData(presentWindowsIndex, enabled, EffectModel::WindowManagementRole);
}

void EffectModel::defaults()
{
    for (int i = 0; i < m_effectsList.count(); ++i) {
        const auto &effect = m_effectsList.at(i);
        if (effect.effectStatus != effect.enabledByDefault) {
            updateEffectStatus(index(i, 0), effect.enabledByDefault);
        }
    }
}

EffectFilterModel::EffectFilterModel(QObject *parent)
    :QSortFilterProxyModel(parent),
    m_effectModel( new EffectModel(this))
{
    setSourceModel(m_effectModel);
}

const QString &EffectFilterModel::filter() const
{
    return m_filter;
}

void EffectFilterModel::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }

    m_filter = filter;
    emit filterChanged();
    invalidateFilter();
}

bool EffectFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
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

    if (m_effectModel->data(index, EffectModel::NameRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    } else if (m_effectModel->data(index, EffectModel::DescriptionRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

void EffectFilterModel::updateEffectStatus(int rowIndex, bool effectState)
{
    const QModelIndex sourceIndex = mapToSource(index(rowIndex, 0));

    m_effectModel->updateEffectStatus(sourceIndex, effectState);
}

void EffectFilterModel::syncConfig()
{
    m_effectModel->syncConfig();
}

void EffectFilterModel::enableWidnowManagement(bool enabled)
{
    m_effectModel->enableWidnowManagement(enabled);
}

void EffectFilterModel::load()
{
    m_effectModel->loadEffects();
}

void EffectFilterModel::defaults()
{
    m_effectModel->defaults();
}

EffectView::EffectView(QWindow *parent)
    : QQuickView(parent)
{
    qmlRegisterType<EffectConfig>("org.kde.kwin.kwincompositing", 1, 0, "EffectConfig");
    qmlRegisterType<EffectFilterModel>("org.kde.kwin.kwincompositing", 1, 0, "EffectFilterModel");
    qmlRegisterType<Compositing>("org.kde.kwin.kwincompositing", 1, 0, "Compositing");
    qmlRegisterType<CompositingType>("org.kde.kwin.kwincompositing", 1, 0, "CompositingType");
    init();
}

void EffectView::init()
{
    QString mainFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwincompositing/qml/main.qml", QStandardPaths::LocateFile);
    setResizeMode(QQuickView::SizeRootObjectToView);
    rootContext()->setContextProperty("engine", this);
    setSource(QUrl(mainFile));
    connect(rootObject(), SIGNAL(changed()), this, SIGNAL(changed()));
}

void EffectView::save()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->syncConfig();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->save();
    }
}

void EffectView::load()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->load();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->reset();
    }
}

void EffectView::defaults()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->defaults();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->defaults();
    }
}

}//end namespace Compositing
}//end namespace KWin

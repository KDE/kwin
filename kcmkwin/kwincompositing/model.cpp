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

#include <QAbstractItemModel>
#include <QHash>
#include <QVariant>
#include <QList>
#include <QString>
#include <QQmlEngine>
#include <QtQml>
#include <QDebug>

#include <KPluginInfo>
#include <KService>
#include <KServiceTypeTrader>
#include <KSharedConfig>

EffectModel::EffectModel(QObject *parent)
    : QAbstractListModel(parent) {

    QHash<int, QByteArray> roleNames;
    roleNames[Name] = "Name";
    roleNames[Description] = "Description";
    roleNames[AuthorName] = "AuthorName";
    roleNames[AuthorEmail] = "AuthorEmail";
    roleNames[License] = "License";
    roleNames[Version] = "Version";
    roleNames[Category] = "Category";
    setRoleNames(roleNames);
    loadEffects();
}

int EffectModel::rowCount(const QModelIndex &parent) const {
    return m_effectsList.count();
}

QVariant EffectModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }

    Effect currentEffect = m_effectsList.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case Name:
            return m_effectsList.at(index.row()).name;
        case Description:
            return m_effectsList.at(index.row()).description;
        case AuthorName:
            return m_effectsList.at(index.row()).authorName;
        case AuthorEmail:
            return m_effectsList.at(index.row()).authorEmail;
        case License:
            return m_effectsList.at(index.row()).license;
        case Version:
            return m_effectsList.at(index.row()).version;
        case Category:
            return m_effectsList.at(index.row()).category;
        default:
            return QVariant();
    }
}

void EffectModel::loadEffects() {
    Effect effect;
    //TODO Until KWin has been ported to frameworks the following code will not work
    //In order to populate our application with some testing data, we are shipping
    //some desktop files with the service folder
    /*KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    foreach (KService::Ptr service, offers) {
        qDebug() << "mesa sto model";
        //data.auroraeName = service->property("X-KDE-PluginInfo-Name").toString();
        //QString scriptName = service->property("X-Plasma-MainScript").toString();
        KPluginInfo plugin(service);

        effect.name = plugin.name();
        effect.description = plugin.comment();
        effect.authorName = plugin.author();
        effect.authorEmail = plugin.email();
        effect.license = plugin.license();
        effect.version = plugin.version();

        m_effectsList << effect;
    }*/
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    QString effectPath = QStandardPaths::locate(QStandardPaths::DataLocation, "service", QStandardPaths::LocateDirectory);

    QDir effectDir(effectPath);

    QStringList effectListDesktop = effectDir.entryList(QDir::Files);
    for(QString effectDekstop : effectListDesktop) {
        KPluginInfo plugin(effectPath + '/' +effectDekstop);
        effect.name = plugin.name();
        effect.description = plugin.comment();
        effect.authorName = plugin.author();
        effect.authorEmail = plugin.email();
        effect.license = plugin.license();
        effect.version = plugin.version();
        effect.category = plugin.category();
        m_effectsList << effect;
    }
    qSort(m_effectsList.begin(), m_effectsList.end(), EffectModel::less);
    endInsertRows();
}

EffectView::EffectView(QWindow *parent)
    : QQuickView(parent)
{
    qmlRegisterType<EffectModel>("org.kde.kwin.kwincompositing", 1, 0, "EffectModel");
    init();
}

void EffectView::init() {
    EffectModel *model = new EffectModel();
    QString mainFile = QStandardPaths::locate(QStandardPaths::DataLocation, "qml/main.qml", QStandardPaths::LocateFile);
    setResizeMode(QQuickView::SizeRootObjectToView);
    rootContext()->setContextProperty("engineObject", this);
    setSource(QUrl(mainFile));
}

void EffectView::effectStatus(const QString &effectName, bool status) {
    m_effectStatus[effectName] = status;
}


bool EffectView::isEnabled(const QString &effectName) {
    KConfigGroup cg(KSharedConfig::openConfig("kwincompositing"), "Plugins");
    return cg.readEntry("kwin4_effect_" + effectName + "_Enabled", false);
}

void EffectView::syncConfig() {
    auto it = m_effectStatus.begin();
    KConfigGroup *kwinConfig = new KConfigGroup(KSharedConfig::openConfig("kwincompositing"), "Plugins");

    while (it != m_effectStatus.end()) {
        QVariant boolToString(it.value());
        kwinConfig->writeEntry("kwin4_effect_" + it.key() + "_Enabled", boolToString.toString());
        it++;
    }
    kwinConfig->sync();
}


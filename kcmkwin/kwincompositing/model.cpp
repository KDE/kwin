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

#include "effectconfig.h"
#include "model.h"

#include <QAbstractItemModel>
#include <QDBusConnection>
#include <QDBusMessage>
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
#include <KCModuleProxy>

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

    beginInsertRows(QModelIndex(), rowCount(), rowCount());

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
        m_effectsList << effect;
    }

    qSort(m_effectsList.begin(), m_effectsList.end(), EffectModel::less);
    endInsertRows();
}

EffectView::EffectView(QWindow *parent)
    : QQuickView(parent)
{
    qmlRegisterType<EffectModel>("org.kde.kwin.kwincompositing", 1, 0, "EffectModel");
    qmlRegisterType<EffectConfig>("org.kde.kwin.kwincompositing", 1, 0, "EffectConfig");

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
    KConfigGroup cg(KSharedConfig::openConfig("kwinrc"), "Plugins");
    QString effectEntry = effectName.toLower().replace(" ", "");
    return cg.readEntry("kwin4_effect_" + effectEntry + "Enabled", false);
}

void EffectView::syncConfig() {
    auto it = m_effectStatus.begin();
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");
    QHash<QString, bool> effectsChanged;

    while (it != m_effectStatus.end()) {
        QVariant boolToString(it.value());
        QString effectName = it.key().toLower();
        QString effectEntry = effectName.replace(" ", "");
        kwinConfig.writeEntry("kwin4_effect_" + effectEntry + "Enabled", boolToString.toString());
        it++;
        effectsChanged["kwin4_effect_" + effectEntry] = boolToString.toBool();
    }
    kwinConfig.sync();

    loadKWinEffects(effectsChanged);
}

void EffectView::loadKWinEffects(const QHash<QString, bool> &effectsChanged) {
    QDBusMessage messageLoadEffect = QDBusMessage::createMethodCall("org.kde.kwin", "/Effects", "org.kde.kwin.Effects", "loadEffect");
    QDBusMessage messageUnloadEffect = QDBusMessage::createMethodCall("org.kde.kwin", "/Effects", "org.kde.kwin.Effects", "unloadEffect");

    auto it = effectsChanged.begin();
    while (it != effectsChanged.end()) {
        bool effectStatus = it.value();
        QString effectEntry = it.key();

        if (effectStatus) {
            messageLoadEffect << effectEntry;
        } else {
            messageUnloadEffect << effectEntry;
        }

        it++;
    }

    QDBusConnection::sessionBus().registerObject("/Effects", this);
    QDBusConnection::sessionBus().send(messageLoadEffect);
    QDBusConnection::sessionBus().send(messageUnloadEffect);
}

QString EffectView::findImage(const QString &imagePath, int size) {
    const QString relativePath("icons/oxygen/" + QString::number(size) + 'x' + QString::number(size) + '/' + imagePath);
    const QString fullImagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, relativePath, QStandardPaths::LocateFile);
    return fullImagePath;
}

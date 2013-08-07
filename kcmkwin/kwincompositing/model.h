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


#ifndef MODEL_H
#define MODEL_H
#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QQuickView>
#include <QString>

namespace KWin {
namespace Compositing {

struct EffectData {
    QString name;
    QString description;
    QString authorName;
    QString authorEmail;
    QString license;
    QString version;
    QString category;
    QString serviceName;
    bool effectStatus;
};

class EffectModel : public QAbstractListModel {

    Q_OBJECT

public:
    enum EffectRoles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        AuthorNameRole,
        AuthorEmailRole,
        LicenseRole,
        VersionRole,
        CategoryRole,
        ServiceNameRole,
        EffectStatusRole
    };

    EffectModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QString serviceName(const QString &effectName);

private:
    void loadEffects();
    QList<EffectData> m_effectsList;

};

class EffectView : public QQuickView {

    Q_OBJECT

public:
    EffectView(QWindow *parent = 0);
    void init();
    void loadKWinEffects(const QHash<QString, bool> &effectsChanged);

    Q_INVOKABLE void effectStatus(const QString &effectName, bool status);
    Q_INVOKABLE void syncConfig();
    Q_INVOKABLE QString findImage(const QString &imagePath, int size = 128);

private:
    QHash<QString, bool> m_effectStatus;
};

}
}
#endif

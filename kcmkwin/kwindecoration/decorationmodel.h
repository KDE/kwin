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
#ifndef KDECORATION_DECORATION_MODEL_H
#define KDECORATION_DECORATION_MODEL_H

#include <QAbstractListModel>

namespace KDecoration2
{

namespace Configuration
{

class DecorationsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit DecorationsModel(QObject *parent = nullptr);
    virtual ~DecorationsModel();

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash< int, QByteArray > roleNames() const override;

    QModelIndex findDecoration(const QString &pluginName, const QString &themeName = QString()) const;

    QMap<QString, QString> knsProviders() const {
        return m_knsProvides;
    }

public Q_SLOTS:
    void init();

private:
    struct Data {
        QString pluginName;
        QString themeName;
        QString visibleName;
        bool configuration = false;
    };
    std::vector<Data> m_plugins;
    QMap<QString, QString> m_knsProvides;
};

}
}

#endif

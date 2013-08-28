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
#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QQuickView>
#include <QSortFilterProxyModel>
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

class EffectModel : public QAbstractItemModel
{

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
        EffectStatusRole,
        WindowManagementRole
    };

    explicit EffectModel(QObject *parent = 0);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QString serviceName(const QString &effectName);
    bool effectListContains(const QString &effectFilter, int source_row);

    virtual QHash< int, QByteArray > roleNames() const override;

    void effectStatus(const QModelIndex &rowIndex, bool effectState);
    QString findImage(const QString &imagePath, int size = 128);
    void reload();
    void syncConfig();
    void enableWidnowManagement(bool enabled);

private:
    void loadEffects();
    void handleDesktopSwitching(int row);
    void handleWindowManagement(int row, bool enabled);
    int findRowByServiceName(const QString &serviceName);
    QList<EffectData> m_effectsList;

};

class EffectView : public QQuickView
{

    Q_OBJECT

public:
    EffectView(QWindow *parent = 0);
    void init();
};


class EffectFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(KWin::Compositing::EffectModel *model READ effectModel WRITE setEffectModel NOTIFY effectModelChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
public:
    EffectFilterModel(QObject *parent = 0);
    EffectModel *effectModel() const;
    const QString &filter() const;

    Q_INVOKABLE void effectStatus(int rowIndex, bool effectState);
    Q_INVOKABLE QString findImage(const QString &imagePath, int size = 128);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void syncConfig();
    Q_INVOKABLE void enableWidnowManagement(bool enabled);

public Q_SLOTS:
    void setEffectModel(EffectModel *effectModel);
    void setFilter(const QString &filter);

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

Q_SIGNALS:
    void effectModelChanged();
    void filterChanged();

private:
    EffectModel *m_effectModel;
    QString m_filter;
};
}
}
#endif

/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAbstractListModel>

namespace KWin
{

class TabBoxLayoutsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum AdditionalRoles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        PluginIdRole,
        PathRole
    };
    Q_ENUM(AdditionalRoles)

    explicit TabBoxLayoutsModel(QObject *parent = nullptr);

    Q_INVOKABLE void load();

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    struct LayoutData
    {
        QString name;
        QString description;
        QString pluginId;
        QString path;
    };

    QList<LayoutData> m_layouts;
};

} // namespace KWin

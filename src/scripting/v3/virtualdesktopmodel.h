/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractListModel>

namespace KWin
{
class VirtualDesktop;

namespace ScriptingModels::V3
{

/**
 * The VirtualDesktopModel class provides a data model for the virtual desktops.
 */
class VirtualDesktopModel : public QAbstractListModel
{
    Q_OBJECT
    /*
     * The number of rows of the grid representation of the virtual desktops.
     * TODO It would be better to make the entire model more like a grid model,
     * with rowCount and columnCount properties.
     */
    Q_PROPERTY(uint gridLayoutRowCount READ gridLayoutRowCount NOTIFY gridLayoutRowCountChanged)

public:
    enum Role {
        DesktopRole = Qt::UserRole + 1,
    };

    explicit VirtualDesktopModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    uint gridLayoutRowCount() const;

public Q_SLOTS:
    void create(uint position, const QString &name = QString());
    void remove(uint position);

Q_SIGNALS:
    void gridLayoutRowCountChanged();

private:
    KWin::VirtualDesktop *desktopFromIndex(const QModelIndex &index) const;

    void handleVirtualDesktopAdded(KWin::VirtualDesktop *desktop);
    void handleVirtualDesktopRemoved(KWin::VirtualDesktop *desktop);
    void handleGridLayoutRowCountChanged(uint rows);

    QVector<KWin::VirtualDesktop *> m_virtualDesktops;
    uint m_gridLayoutRowCount;
};

} // namespace ScriptingModels::V3
} // namespace KWin

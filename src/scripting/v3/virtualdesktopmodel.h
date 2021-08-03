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

public:
    enum Role {
        DesktopRole = Qt::UserRole + 1,
    };

    explicit VirtualDesktopModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public Q_SLOTS:
    void create(uint position, const QString &name = QString());
    void remove(uint position);

private:
    KWin::VirtualDesktop *desktopFromIndex(const QModelIndex &index) const;

    void handleVirtualDesktopAdded(KWin::VirtualDesktop *desktop);
    void handleVirtualDesktopRemoved(KWin::VirtualDesktop *desktop);

    QVector<KWin::VirtualDesktop *> m_virtualDesktops;
};

} // namespace ScriptingModels::V3
} // namespace KWin

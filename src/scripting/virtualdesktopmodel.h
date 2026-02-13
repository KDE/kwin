/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractListModel>

namespace KWin
{
class VirtualDesktop;

/*!
 * \qmltype VirtualDesktopModel
 * \inqmlmodule org.kde.kwin
 *
 * \brief Provides a data model for the virtual desktops.
 *
 * This model provides the following roles:
 * \list
 * \li desktop (VirtualDesktop)
 * \endlist
 *
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
    /*!
     * \qmlmethod VirtualDesktop VirtualDesktopModel::create(int position, string name)
     */
    KWin::VirtualDesktop *create(uint position, const QString &name = QString());

    /*!
     * \qmlmethod void VirtualDesktopModel::remove(int position)
     */
    void remove(uint position);

private:
    KWin::VirtualDesktop *desktopFromIndex(const QModelIndex &index) const;

    void handleVirtualDesktopAdded(KWin::VirtualDesktop *desktop);
    void handleVirtualDesktopRemoved(KWin::VirtualDesktop *desktop);
    void handleVirtualDesktopMoved(KWin::VirtualDesktop *desktop, int position);

    QList<KWin::VirtualDesktop *> m_virtualDesktops;
};

} // namespace KWin

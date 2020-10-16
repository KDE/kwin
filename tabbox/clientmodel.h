/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CLIENTMODEL_H
#define CLIENTMODEL_H
#include "tabboxhandler.h"

#include <QModelIndex>
/**
 * @file
 * This file defines the class ClientModel, the model for TabBoxClients.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.4
 */

namespace KWin
{
namespace TabBox
{


/**
 * The model for TabBoxClients used in TabBox.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.4
 */
class ClientModel
    : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum {
        ClientRole = Qt::UserRole, ///< The TabBoxClient
        CaptionRole = Qt::UserRole + 1, ///< The caption of TabBoxClient
        DesktopNameRole = Qt::UserRole + 2, ///< The name of the desktop the TabBoxClient is on
        IconRole = Qt::UserRole + 3, // TODO: to be removed
        WIdRole = Qt::UserRole + 5, ///< The window ID of TabBoxClient
        MinimizedRole = Qt::UserRole + 6, ///< TabBoxClient is minimized
        CloseableRole = Qt::UserRole + 7 ///< TabBoxClient can be closed
    };
    explicit ClientModel(QObject* parent = nullptr);
    ~ClientModel() override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QString longestCaption() const;

    /**
     * @param client The TabBoxClient whose index should be returned
     * @return Returns the ModelIndex of given TabBoxClient or an invalid ModelIndex
     * if the model does not contain the given TabBoxClient.
     */
    QModelIndex index(QWeakPointer<TabBoxClient> client) const;

    /**
     * Generates a new list of TabBoxClients based on the current config.
     * Calling this method will reset the model. If partialReset is true
     * the top of the list is kept as a starting point. If not the
     * current active client is used as the starting point to generate the
     * list.
     * @param desktop The desktop for which the list should be created
     * @param partialReset Keep the currently selected client or regenerate everything
     */
    void createClientList(int desktop, bool partialReset = false);
    /**
     * This method is provided as a overload for current desktop
     * @see createClientList
     */
    void createClientList(bool partialReset = false);
    /**
     * @return Returns the current list of TabBoxClients.
     */
    TabBoxClientList clientList() const {
        return m_clientList;
    }

public Q_SLOTS:
    void close(int index);
    /**
     * Activates the client at @p index and closes the TabBox.
     * @param index The row index
     */
    void activate(int index);

private:
    TabBoxClientList m_clientList;
};

} // namespace Tabbox
} // namespace KWin

#endif // CLIENTMODEL_H

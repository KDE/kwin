/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "tabboxhandler.h"

#include <QModelIndex>
/**
 * @file
 * This file defines the class ClientModel, the model for Windows.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.4
 */

namespace KWin
{
namespace TabBox
{

/**
 * The model for Windows used in TabBox.
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
        ClientRole = Qt::UserRole, ///< The Window
        CaptionRole = Qt::UserRole + 1, ///< The caption of Window
        DesktopNameRole = Qt::UserRole + 2, ///< The name of the desktop the Window is on
        IconRole = Qt::UserRole + 3, // TODO: to be removed
        WIdRole = Qt::UserRole + 5, ///< The window ID of Window
        MinimizedRole = Qt::UserRole + 6, ///< Window is minimized
        CloseableRole = Qt::UserRole + 7 ///< Window can be closed
    };
    explicit ClientModel(QObject *parent = nullptr);
    ~ClientModel() override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QString longestCaption() const;

    /**
     * @param client The Window whose index should be returned
     * @return Returns the ModelIndex of given Window or an invalid ModelIndex
     * if the model does not contain the given Window.
     */
    QModelIndex index(Window *client) const;

    /**
     * Generates a new list of Windows based on the current config.
     * Calling this method will reset the model. If partialReset is true
     * the top of the list is kept as a starting point. If not the
     * current active client is used as the starting point to generate the
     * list.
     * @param partialReset Keep the currently selected client or regenerate everything
     */
    void createClientList(bool partialReset = false);
    /**
     * @return Returns the current list of Windows.
     */
    QList<Window *> clientList() const
    {
        return m_mutableClientList;
    }

public Q_SLOTS:
    void close(int index);
    /**
     * Activates the client at @p index and closes the TabBox.
     * @param index The row index
     */
    void activate(int index);

private:
    void createFocusChainClientList(Window *start);
    void createStackingOrderClientList(Window *start);

    QList<Window *> m_clientList;
    QList<Window *> m_mutableClientList;
};

} // namespace Tabbox
} // namespace KWin

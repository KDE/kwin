/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include <optional>

namespace KWin
{
class AbstractClient;

namespace ScriptingModels::V3
{

class ClientModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ClientRole = Qt::UserRole + 1,
        ScreenRole,
        DesktopRole,
        ActivityRole
    };

    explicit ClientModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

private:
    void markRoleChanged(AbstractClient *client, int role);

    void handleClientAdded(AbstractClient *client);
    void handleClientRemoved(AbstractClient *client);
    void setupClientConnections(AbstractClient *client);

    QList<AbstractClient *> m_clients;
};

class ClientFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(ClientModel *clientModel READ clientModel WRITE setClientModel NOTIFY clientModelChanged)
    Q_PROPERTY(QString activity READ activity WRITE setActivity RESET resetActivity NOTIFY activityChanged)
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop RESET resetDesktop NOTIFY desktopChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString screenName READ screenName WRITE setScreenName RESET resetScreenName NOTIFY screenNameChanged)
    Q_PROPERTY(WindowTypes windowType READ windowType WRITE setWindowType RESET resetWindowType NOTIFY windowTypeChanged)

public:
    enum WindowType {
        Normal = 0x1,
        Dock = 0x2,
        Desktop = 0x4,
        Notification = 0x8,
        CriticalNotification = 0x10,
    };
    Q_DECLARE_FLAGS(WindowTypes, WindowType)
    Q_FLAG(WindowTypes)

    explicit ClientFilterModel(QObject *parent = nullptr);

    ClientModel *clientModel() const;
    void setClientModel(ClientModel *clientModel);

    QString activity() const;
    void setActivity(const QString &activity);
    void resetActivity();

    int desktop() const;
    void setDesktop(int desktop);
    void resetDesktop();

    QString filter() const;
    void setFilter(const QString &filter);

    QString screenName() const;
    void setScreenName(const QString &screenName);
    void resetScreenName();

    WindowTypes windowType() const;
    void setWindowType(WindowTypes windowType);
    void resetWindowType();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

Q_SIGNALS:
    void activityChanged();
    void desktopChanged();
    void screenNameChanged();
    void clientModelChanged();
    void filterChanged();
    void windowTypeChanged();

private:
    WindowTypes windowTypeMask(AbstractClient *client) const;

    ClientModel *m_clientModel = nullptr;
    std::optional<QString> m_activity;
    std::optional<int> m_desktop;
    QString m_filter;
    std::optional<QString> m_screenName;
    std::optional<WindowTypes> m_windowType;
};

} // namespace ScriptingModels::V3
} // namespace KWin

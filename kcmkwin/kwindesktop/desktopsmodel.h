/*
    SPDX-FileCopyrightText: 2018 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DESKTOPSMODEL_H
#define DESKTOPSMODEL_H

#include <QAbstractListModel>

#include "../virtualdesktopsdbustypes.h"

class QDBusArgument;
class QDBusMessage;
class QDBusServiceWatcher;


namespace KWin
{

/**
 * @short An item model around KWin's D-Bus API for virtual desktops.
 *
 * The model initially gets the state from KWin and populates.
 *
 * As long as the user makes no changes, KWin-side changes are directly
 * exposed in the model.
 *
 * If the user makes changes (see the `userModified` property), it stops
 * exposing KWin-side changes live, but it keeps track of the KWin-side
 * changes, so it can figure out and apply the delta when `syncWithServer`
 * is called.
 *
 * When KWin-side changes happen while the model is user-modified, the
 * model signals this via the `serverModified` property. A call to
 * `syncWithServer` will overwrite the KWin-side changes.
 *
 * After synchronization, the model tracks Kwin-side changes again,
 * until the user makes further changes.
 *
 * @author Eike Hein <hein@kde.org>
 */

class DesktopsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool userModified READ userModified NOTIFY userModifiedChanged)
    Q_PROPERTY(bool serverModified READ serverModified NOTIFY serverModifiedChanged)
    Q_PROPERTY(int rows READ rows WRITE setRows NOTIFY rowsChanged)

public:
    enum AdditionalRoles {
        Id = Qt::UserRole + 1,
        DesktopRow
    };
    Q_ENUM(AdditionalRoles)

    explicit DesktopsModel(QObject *parent = nullptr);
    ~DesktopsModel() override;

    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = {}) const override;

    bool ready() const;
    QString error() const;

    bool userModified() const;
    bool serverModified() const;

    int rows() const;
    void setRows(int rows);

    Q_INVOKABLE void createDesktop(const QString &name);
    Q_INVOKABLE void removeDesktop(const QString &id);
    Q_INVOKABLE void setDesktopName(const QString &id, const QString &name);

    Q_INVOKABLE void syncWithServer();

    bool needsSave() const;
    void load();
    void defaults();
    bool isDefaults() const;

Q_SIGNALS:
    void readyChanged() const;
    void errorChanged() const;
    void userModifiedChanged() const;
    void serverModifiedChanged() const;
    void rowsChanged() const;

protected Q_SLOTS:
    void reset();
    void getAllAndConnect(const QDBusMessage &msg);
    void desktopCreated(const QString &id, const KWin::DBusDesktopDataStruct &data);
    void desktopRemoved(const QString &id);
    void desktopDataChanged(const QString &id, const KWin::DBusDesktopDataStruct &data);
    void desktopRowsChanged(uint rows);
    void updateModifiedState(bool server = false);
    void handleCallError();

private:
    QDBusServiceWatcher *m_serviceWatcher;
    QString m_error;
    bool m_userModified;
    bool m_serverModified;
    QStringList m_serverSideDesktops;
    QHash<QString,QString> m_serverSideNames;
    int m_serverSideRows;
    QStringList m_desktops;
    QHash<QString,QString> m_names;
    int m_rows;
    bool m_synchronizing;
};

}

#endif

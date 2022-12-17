/*
    SPDX-FileCopyrightText: 2018 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopsmodel.h"

#include <cmath>

#include <KLocalizedString>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QUuid>

namespace KWin
{

static const QString s_serviceName(QStringLiteral("org.kde.KWin"));
static const QString s_virtualDesktopsInterface(QStringLiteral("org.kde.KWin.VirtualDesktopManager"));
static const QString s_virtDesktopsPath(QStringLiteral("/VirtualDesktopManager"));
static const QString s_fdoPropertiesInterface(QStringLiteral("org.freedesktop.DBus.Properties"));

DesktopsModel::DesktopsModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_userModified(false)
    , m_serverModified(false)
    , m_serverSideRows(-1)
    , m_rows(-1)
{
    qDBusRegisterMetaType<KWin::DBusDesktopDataStruct>();
    qDBusRegisterMetaType<KWin::DBusDesktopDataVector>();

    m_serviceWatcher = new QDBusServiceWatcher(s_serviceName,
                                               QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange);

    QObject::connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered,
                     this, [this]() {
                         reset();
                     });

    QObject::connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
                     this, [this]() {
                         QDBusConnection::sessionBus().disconnect(
                             s_serviceName,
                             s_virtDesktopsPath,
                             s_virtualDesktopsInterface,
                             QStringLiteral("desktopCreated"),
                             this,
                             SLOT(desktopCreated(QString, KWin::DBusDesktopDataStruct)));

                         QDBusConnection::sessionBus().disconnect(
                             s_serviceName,
                             s_virtDesktopsPath,
                             s_virtualDesktopsInterface,
                             QStringLiteral("desktopRemoved"),
                             this,
                             SLOT(desktopRemoved(QString)));

                         QDBusConnection::sessionBus().disconnect(
                             s_serviceName,
                             s_virtDesktopsPath,
                             s_virtualDesktopsInterface,
                             QStringLiteral("desktopDataChanged"),
                             this,
                             SLOT(desktopDataChanged(QString, KWin::DBusDesktopDataStruct)));

                         QDBusConnection::sessionBus().disconnect(
                             s_serviceName,
                             s_virtDesktopsPath,
                             s_virtualDesktopsInterface,
                             QStringLiteral("rowsChanged"),
                             this,
                             SLOT(desktopRowsChanged(uint)));
                     });

    reset();
}

DesktopsModel::~DesktopsModel()
{
}

QHash<int, QByteArray> DesktopsModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();

    QMetaEnum e = metaObject()->enumerator(metaObject()->indexOfEnumerator("AdditionalRoles"));

    for (int i = 0; i < e.keyCount(); ++i) {
        roles.insert(e.value(i), e.key(i));
    }

    return roles;
}

QVariant DesktopsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() > (m_desktops.count() - 1)) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        return m_names.value(m_desktops.at(index.row()));
    } else if (role == Id) {
        return m_desktops.at(index.row());
    } else if (role == DesktopRow) {
        const int rows = std::max(m_rows, 1);
        const int perRow = std::ceil((qreal)m_desktops.count() / (qreal)rows);

        return (index.row() / perRow) + 1;

    } else if (role == IsDefault) {
        // According to defaults(), first desktop is default
        return index.row() == 0;
    }

    return QVariant();
}

int DesktopsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_desktops.count();
}

bool DesktopsModel::ready() const
{
    return !m_desktops.isEmpty();
}

QString DesktopsModel::error() const
{
    return m_error;
}

bool DesktopsModel::userModified() const
{
    return m_userModified;
}

bool DesktopsModel::serverModified() const
{
    return m_serverModified;
}

int DesktopsModel::rows() const
{
    return m_rows;
}

void DesktopsModel::setRows(int rows)
{
    if (!ready()) {
        return;
    }

    if (m_rows != rows) {
        m_rows = rows;

        Q_EMIT rowsChanged();
        Q_EMIT dataChanged(index(0, 0), index(m_desktops.count() - 1, 0), QVector<int>{DesktopRow});

        updateModifiedState();
    }
}

int DesktopsModel::desktopCount() const
{
    return rowCount();
}

QString DesktopsModel::createDesktopName() const
{
    const QStringList nameValues = m_names.values();
    for (int index = 1;; ++index) {
        const QString desktopName = i18ncp("A numbered name for virtual desktops", "Desktop %1", "Desktop %1", index);
        if (!nameValues.contains(desktopName)) {
            return desktopName;
        }
    }
}

void DesktopsModel::createDesktop()
{
    if (!ready()) {
        return;
    }

    beginInsertRows(QModelIndex(), m_desktops.count(), m_desktops.count());

    const QString &dummyId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_desktops.append(dummyId);
    m_names[dummyId] = createDesktopName();

    endInsertRows();
    Q_EMIT desktopCountChanged();

    updateModifiedState();
}

void DesktopsModel::removeDesktop(const QString &id)
{
    if (!ready() || !m_desktops.contains(id)) {
        return;
    }

    const int desktopIndex = m_desktops.indexOf(id);

    beginRemoveRows(QModelIndex(), desktopIndex, desktopIndex);

    m_desktops.removeAt(desktopIndex);
    m_names.remove(id);

    endRemoveRows();
    Q_EMIT desktopCountChanged();

    updateModifiedState();
}

void DesktopsModel::setDesktopName(const QString &id, const QString &name)
{
    if (!ready() || !m_desktops.contains(id)) {
        return;
    }

    m_names[id] = name;

    const QModelIndex &idx = index(m_desktops.indexOf(id), 0);

    Q_EMIT dataChanged(idx, idx, QVector<int>{Qt::DisplayRole});

    updateModifiedState();
}

void DesktopsModel::syncWithServer()
{
    auto callFinished = [this](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<void> reply = *call;

        if (reply.isError()) {
            handleCallError();
        }

        --m_pendingCalls;

        call->deleteLater();
    };

    if (m_desktops.count() > m_serverSideDesktops.count()) {
        auto call = QDBusMessage::createMethodCall(
            s_serviceName,
            s_virtDesktopsPath,
            s_virtualDesktopsInterface,
            QStringLiteral("createDesktop"));

        const int newIndex = m_serverSideDesktops.count();

        call.setArguments({(uint)newIndex, m_names.value(m_desktops.at(newIndex))});

        ++m_pendingCalls;
        QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(call);

        const auto *watcher = new QDBusPendingCallWatcher(pending, this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, callFinished);

        return; // The change-handling slot will call syncWithServer() again,
                // until everything is in sync.
    }

    if (m_desktops.count() < m_serverSideDesktops.count()) {
        QStringListIterator i(m_serverSideDesktops);

        i.toBack();

        while (i.hasPrevious()) {
            const QString &previous = i.previous();

            if (!m_desktops.contains(previous)) {
                auto call = QDBusMessage::createMethodCall(
                    s_serviceName,
                    s_virtDesktopsPath,
                    s_virtualDesktopsInterface,
                    QStringLiteral("removeDesktop"));

                call.setArguments({previous});

                ++m_pendingCalls;
                QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(call);

                const QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pending, this);
                QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, callFinished);

                return; // The change-handling slot will call syncWithServer() again,
                        // until everything is in sync.
            }
        }
    }

    // Sync ids. Replace dummy ids in the process.
    for (int i = 0; i < m_serverSideDesktops.count(); ++i) {
        const QString oldId = m_desktops.at(i);
        const QString &newId = m_serverSideDesktops.at(i);
        m_desktops[i] = newId;
        m_names[newId] = m_names.take(oldId);
    }

    Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), QVector<int>{Qt::DisplayRole});

    // Sync names.
    if (m_names != m_serverSideNames) {
        QHashIterator<QString, QString> i(m_names);

        while (i.hasNext()) {
            i.next();

            if (i.value() != m_serverSideNames.value(i.key())) {
                auto call = QDBusMessage::createMethodCall(
                    s_serviceName,
                    s_virtDesktopsPath,
                    s_virtualDesktopsInterface,
                    QStringLiteral("setDesktopName"));

                call.setArguments({i.key(), i.value()});

                ++m_pendingCalls;
                QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(call);

                const QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pending, this);
                QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, callFinished);

                break;
            }
        }

        return; // The change-handling slot will call syncWithServer() again,
                // until everything is in sync..
    }

    // Sync rows.
    if (m_rows != m_serverSideRows) {
        auto call = QDBusMessage::createMethodCall(
            s_serviceName,
            s_virtDesktopsPath,
            s_fdoPropertiesInterface,
            QStringLiteral("Set"));

        call.setArguments({s_virtualDesktopsInterface,
                           QStringLiteral("rows"), QVariant::fromValue(QDBusVariant(QVariant((uint)m_rows)))});

        ++m_pendingCalls;
        QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(call);

        const QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pending, this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, callFinished);
    }
}

void DesktopsModel::reset()
{
    auto getAllAndConnectCall = QDBusMessage::createMethodCall(
        s_serviceName,
        s_virtDesktopsPath,
        s_fdoPropertiesInterface,
        QStringLiteral("GetAll"));

    getAllAndConnectCall.setArguments({s_virtualDesktopsInterface});

    QDBusConnection::sessionBus().callWithCallback(
        getAllAndConnectCall,
        this,
        SLOT(getAllAndConnect(QDBusMessage)),
        SLOT(handleCallError()));
}

bool DesktopsModel::needsSave() const
{
    return m_userModified;
}

bool DesktopsModel::isDefaults() const
{
    return m_rows == 2 && m_desktops.count() == 1;
}

void DesktopsModel::defaults()
{
    beginResetModel();
    // default is 1 desktop with 2 rows
    // see kwin/virtualdesktops.cpp  VirtualDesktopGrid::VirtualDesktopGrid
    while (m_desktops.count() > 1) {
        const auto desktop = m_desktops.takeLast();
        m_names.remove(desktop);
    }
    setRows(2);

    endResetModel();

    m_userModified = true;
    updateModifiedState();
}

void DesktopsModel::load()
{
    beginResetModel();
    m_desktops = m_serverSideDesktops;
    m_names = m_serverSideNames;
    setRows(m_serverSideRows);
    endResetModel();

    m_userModified = true;
    updateModifiedState();
}

void DesktopsModel::getAllAndConnect(const QDBusMessage &msg)
{
    const QVariantMap &data = qdbus_cast<QVariantMap>(msg.arguments().at(0).value<QDBusArgument>());

    const KWin::DBusDesktopDataVector &desktops = qdbus_cast<KWin::DBusDesktopDataVector>(
        data.value(QStringLiteral("desktops")).value<QDBusArgument>());

    const int newServerSideRows = data.value(QStringLiteral("rows")).toUInt();
    QStringList newServerSideDesktops;
    QHash<QString, QString> newServerSideNames;

    for (const KWin::DBusDesktopDataStruct &d : desktops) {
        newServerSideDesktops.append(d.id);
        newServerSideNames[d.id] = d.name;
    }

    // If the server-side state changed during a KWin restart, and the
    // user had made notifications, the model should notify about the
    // change.
    if (m_serverSideDesktops != newServerSideDesktops
        || m_serverSideNames != newServerSideNames
        || m_serverSideRows != newServerSideRows) {
        if (!m_serverSideDesktops.isEmpty() || m_userModified) {
            m_serverModified = true;
            Q_EMIT serverModifiedChanged();
        }

        m_serverSideDesktops = newServerSideDesktops;
        m_serverSideNames = newServerSideNames;
        m_serverSideRows = newServerSideRows;
    }

    // For the case KWin restarts while the KCM was open: If the user had
    // made no modifications, just reset to the server data. E.g. perhaps
    // the user intentionally nuked the KWin config while it was down, so
    // we should follow.
    if (!m_userModified || m_desktops.empty()) {
        beginResetModel();
        m_desktops = m_serverSideDesktops;
        m_names = m_serverSideNames;
        m_rows = m_serverSideRows;
        endResetModel();

        Q_EMIT rowsChanged();
    }

    Q_EMIT readyChanged();

    auto handleConnectionError = [this]() {
        m_error = i18n("There was an error connecting to the compositor.");
        Q_EMIT errorChanged();
    };

    bool connected = QDBusConnection::sessionBus().connect(
        s_serviceName,
        s_virtDesktopsPath,
        s_virtualDesktopsInterface,
        QStringLiteral("desktopCreated"),
        this,
        SLOT(desktopCreated(QString, KWin::DBusDesktopDataStruct)));

    if (!connected) {
        handleConnectionError();

        return;
    }

    connected = QDBusConnection::sessionBus().connect(
        s_serviceName,
        s_virtDesktopsPath,
        s_virtualDesktopsInterface,
        QStringLiteral("desktopRemoved"),
        this,
        SLOT(desktopRemoved(QString)));

    if (!connected) {
        handleConnectionError();

        return;
    }

    connected = QDBusConnection::sessionBus().connect(
        s_serviceName,
        s_virtDesktopsPath,
        s_virtualDesktopsInterface,
        QStringLiteral("desktopDataChanged"),
        this,
        SLOT(desktopDataChanged(QString, KWin::DBusDesktopDataStruct)));

    if (!connected) {
        handleConnectionError();

        return;
    }

    connected = QDBusConnection::sessionBus().connect(
        s_serviceName,
        s_virtDesktopsPath,
        s_virtualDesktopsInterface,
        QStringLiteral("rowsChanged"),
        this,
        SLOT(desktopRowsChanged(uint)));

    if (!connected) {
        handleConnectionError();

        return;
    }
}

void DesktopsModel::desktopCreated(const QString &id, const KWin::DBusDesktopDataStruct &data)
{
    m_serverSideDesktops.insert(data.position, id);
    m_serverSideNames[data.id] = data.name;

    // If the user didn't make any changes, we can just stay in sync.
    if (!m_userModified) {
        beginInsertRows(QModelIndex(), data.position, data.position);

        m_desktops = m_serverSideDesktops;
        m_names = m_serverSideNames;

        endInsertRows();
    } else {
        // Remove dummy data.
        const QString dummyId = m_desktops.at(data.position);
        m_desktops[data.position] = id;
        m_names.remove(dummyId);
        m_names[id] = data.name;
        const QModelIndex &idx = index(data.position, 0);
        Q_EMIT dataChanged(idx, idx, QVector<int>{Id});

        updateModifiedState(/* server */ true);
    }
}

void DesktopsModel::desktopRemoved(const QString &id)
{
    const int desktopIndex = m_serverSideDesktops.indexOf(id);

    m_serverSideDesktops.removeAt(desktopIndex);
    m_serverSideNames.remove(id);

    // If the user didn't make any changes, we can just stay in sync.
    if (!m_userModified) {
        beginRemoveRows(QModelIndex(), desktopIndex, desktopIndex);

        m_desktops = m_serverSideDesktops;
        m_names = m_serverSideNames;

        endRemoveRows();
    } else {
        updateModifiedState(/* server */ true);
    }
}

void DesktopsModel::desktopDataChanged(const QString &id, const KWin::DBusDesktopDataStruct &data)
{
    const int desktopIndex = m_serverSideDesktops.indexOf(id);

    m_serverSideDesktops[desktopIndex] = id;
    m_serverSideNames[id] = data.name;

    // If the user didn't make any changes, we can just stay in sync.
    if (!m_userModified) {
        m_desktops = m_serverSideDesktops;
        m_names = m_serverSideNames;

        const QModelIndex &idx = index(desktopIndex, 0);

        Q_EMIT dataChanged(idx, idx, QVector<int>{Qt::DisplayRole});
    } else {
        updateModifiedState(/* server */ true);
    }
}

void DesktopsModel::desktopRowsChanged(uint rows)
{
    // Unfortunately we sometimes get this signal from the server with an unchanged value.
    if ((int)rows == m_serverSideRows) {
        return;
    }

    m_serverSideRows = rows;

    // If the user didn't make any changes, we can just stay in sync.
    if (!m_userModified) {
        m_rows = m_serverSideRows;

        Q_EMIT rowsChanged();
        Q_EMIT dataChanged(index(0, 0), index(m_desktops.count() - 1, 0), QVector<int>{DesktopRow});
    } else {
        updateModifiedState(/* server */ true);
    }
}

void DesktopsModel::updateModifiedState(bool server)
{
    // Count is the same but contents are not: The user may have
    // removed and created new desktops in the UI, but there were
    // no changes to send to the server because number and names
    // have remained the same. In that case we can just clean
    // that up here.
    if (m_desktops.count() == m_serverSideDesktops.count()
        && m_desktops != m_serverSideDesktops) {

        for (int i = 0; i < m_serverSideDesktops.count(); ++i) {
            const QString oldId = m_desktops.at(i);
            const QString &newId = m_serverSideDesktops.at(i);
            m_desktops[i] = newId;
            m_names[newId] = m_names.take(oldId);
        }

        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), QVector<int>{Qt::DisplayRole});
    }

    if (m_desktops == m_serverSideDesktops
        && m_names == m_serverSideNames
        && m_rows == m_serverSideRows) {

        m_userModified = false;
        Q_EMIT userModifiedChanged();

        m_serverModified = false;
        Q_EMIT serverModifiedChanged();
    } else {
        if (m_pendingCalls > 0) {
            m_serverModified = false;
            Q_EMIT serverModifiedChanged();

            syncWithServer();
        } else if (server) {
            m_serverModified = true;
            Q_EMIT serverModifiedChanged();
        } else {
            m_userModified = true;
            Q_EMIT userModifiedChanged();
        }
    }
}

void DesktopsModel::handleCallError()
{
    if (m_pendingCalls > 0) {

        m_serverModified = false;
        Q_EMIT serverModifiedChanged();

        m_error = i18n("There was an error saving the settings to the compositor.");
        Q_EMIT errorChanged();
    } else {
        m_error = i18n("There was an error requesting information from the compositor.");
        Q_EMIT errorChanged();
    }
}

}

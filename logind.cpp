/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "logind.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDBusMetaType>

#include <sys/stat.h>
#include <config-kwin.h>
#if HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif
#ifndef major
#include <sys/types.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include "utils.h"

struct DBusLogindSeat {
    QString name;
    QDBusObjectPath path;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusLogindSeat &seat)
{
    argument.beginStructure();
    argument << seat.name << seat.path ;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusLogindSeat &seat)
{
    argument.beginStructure();
    argument >> seat.name >> seat.path;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(DBusLogindSeat)

namespace KWin
{

const static QString s_login1Name = QStringLiteral("logind");
const static QString s_login1Service = QStringLiteral("org.freedesktop.login1");
const static QString s_login1Path = QStringLiteral("/org/freedesktop/login1");
const static QString s_login1ManagerInterface = QStringLiteral("org.freedesktop.login1.Manager");
const static QString s_login1SeatInterface = QStringLiteral("org.freedesktop.login1.Seat");
const static QString s_login1SessionInterface = QStringLiteral("org.freedesktop.login1.Session");
const static QString s_login1ActiveProperty = QStringLiteral("Active");

const static QString s_ck2Name = QStringLiteral("ConsoleKit");
const static QString s_ck2Service = QStringLiteral("org.freedesktop.ConsoleKit");
const static QString s_ck2Path = QStringLiteral("/org/freedesktop/ConsoleKit/Manager");
const static QString s_ck2ManagerInterface = QStringLiteral("org.freedesktop.ConsoleKit.Manager");
const static QString s_ck2SeatInterface = QStringLiteral("org.freedesktop.ConsoleKit.Seat");
const static QString s_ck2SessionInterface = QStringLiteral("org.freedesktop.ConsoleKit.Session");
const static QString s_ck2ActiveProperty = QStringLiteral("active");

const static QString s_dbusPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");



LogindIntegration *LogindIntegration::s_self = nullptr;

LogindIntegration *LogindIntegration::create(QObject *parent)
{
    Q_ASSERT(!s_self);
    s_self = new LogindIntegration(parent);
    return s_self;
}

LogindIntegration::LogindIntegration(const QDBusConnection &connection, QObject *parent)
    : QObject(parent)
    , m_bus(connection)
    , m_connected(false)
    , m_sessionControl(false)
    , m_sessionActive(false)
{
    // check whether the logind service is registered
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                                          QStringLiteral("/"),
                                                          QStringLiteral("org.freedesktop.DBus"),
                                                          QStringLiteral("ListNames"));
    QDBusPendingReply<QStringList> async = m_bus.asyncCall(message);
    QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(async, this);
    connect(callWatcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *self) {
            QDBusPendingReply<QStringList> reply = *self;
            self->deleteLater();
            if (!reply.isValid()) {
                return;
            }
            if (reply.value().contains(s_login1Service)) {
                setupSessionController(SessionControllerLogind);
            } else if (reply.value().contains(s_ck2Service)) {
                setupSessionController(SessionControllerConsoleKit);
            }

        }
    );
}

LogindIntegration::LogindIntegration(QObject *parent)
    : LogindIntegration(QDBusConnection::systemBus(), parent)
{
}

LogindIntegration::~LogindIntegration()
{
    s_self = nullptr;
}

void LogindIntegration::setupSessionController(SessionController controller)
{
    if (controller == SessionControllerLogind) {
        // We have the logind serivce, set it up and use it
        m_sessionControllerName = s_login1Name;
        m_sessionControllerService = s_login1Service;
        m_sessionControllerPath = s_login1Path;
        m_sessionControllerManagerInterface = s_login1ManagerInterface;
        m_sessionControllerSeatInterface = s_login1SeatInterface;
        m_sessionControllerSessionInterface = s_login1SessionInterface;
        m_sessionControllerActiveProperty = s_login1ActiveProperty;
        m_logindServiceWatcher = new QDBusServiceWatcher(m_sessionControllerService,
                                                         m_bus,
                                                         QDBusServiceWatcher::WatchForUnregistration | QDBusServiceWatcher::WatchForRegistration,
                                                         this);
        connect(m_logindServiceWatcher, &QDBusServiceWatcher::serviceRegistered, this, &LogindIntegration::logindServiceRegistered);
        connect(m_logindServiceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
                [this]() {
                    m_connected = false;
                    emit connectedChanged();
                }
        );
        logindServiceRegistered();
    } else if (controller == SessionControllerConsoleKit) {
        // We have the ConsoleKit serivce, set it up and use it
        m_sessionControllerName = s_ck2Name;
        m_sessionControllerService = s_ck2Service;
        m_sessionControllerPath = s_ck2Path;
        m_sessionControllerManagerInterface = s_ck2ManagerInterface;
        m_sessionControllerSeatInterface = s_ck2SeatInterface;
        m_sessionControllerSessionInterface = s_ck2SessionInterface;
        m_sessionControllerActiveProperty = s_ck2ActiveProperty;
        m_logindServiceWatcher = new QDBusServiceWatcher(m_sessionControllerService,
                                                         m_bus,
                                                         QDBusServiceWatcher::WatchForUnregistration | QDBusServiceWatcher::WatchForRegistration,
                                                         this);
        connect(m_logindServiceWatcher, &QDBusServiceWatcher::serviceRegistered, this, &LogindIntegration::logindServiceRegistered);
        connect(m_logindServiceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
                [this]() {
                    m_connected = false;
                    emit connectedChanged();
                }
        );
        logindServiceRegistered();
    }
}

void LogindIntegration::logindServiceRegistered()
{
    const QByteArray sessionId = qgetenv("XDG_SESSION_ID");
    QString methodName;
    QVariantList args;
    if (sessionId.isEmpty()) {
        methodName = QStringLiteral("GetSessionByPID");
        args << (quint32) QCoreApplication::applicationPid();
    } else {
        methodName = QStringLiteral("GetSession");
        args << QString::fromLocal8Bit(sessionId);
    }
    // get the current session
    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionControllerPath,
                                                          m_sessionControllerManagerInterface,
                                                          methodName);
    message.setArguments(args);
    QDBusPendingReply<QDBusObjectPath> session = m_bus.asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(session, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *self) {
            QDBusPendingReply<QDBusObjectPath> reply = *self;
            self->deleteLater();
            if (m_connected) {
                return;
            }
            if (!reply.isValid()) {
                qCDebug(KWIN_CORE) << "The session is not registered with " << m_sessionControllerName << " " << reply.error().message();
                return;
            }
            m_sessionPath = reply.value().path();
            qCDebug(KWIN_CORE) << "Session path:" << m_sessionPath;
            m_connected = true;
            connectSessionPropertiesChanged();
            // activate the session, in case we are not on it
            QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                                m_sessionPath,
                                                                m_sessionControllerSessionInterface,
                                                                QStringLiteral("Activate"));
            // blocking on purpose
            m_bus.call(message);
            getSeat();
            getSessionActive();
            getVirtualTerminal();

            emit connectedChanged();
        }
    );
}

void LogindIntegration::connectSessionPropertiesChanged()
{
    m_bus.connect(m_sessionControllerService,
                  m_sessionPath,
                  s_dbusPropertiesInterface,
                  QStringLiteral("PropertiesChanged"),
                  this,
                  SLOT(getSessionActive()));
    m_bus.connect(m_sessionControllerService,
                  m_sessionPath,
                  s_dbusPropertiesInterface,
                  QStringLiteral("PropertiesChanged"),
                  this,
                  SLOT(getVirtualTerminal()));
}

void LogindIntegration::getSessionActive()
{
    if (!m_connected || m_sessionPath.isEmpty()) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionPath,
                                                          s_dbusPropertiesInterface,
                                                          QStringLiteral("Get"));
    message.setArguments(QVariantList({m_sessionControllerSessionInterface, m_sessionControllerActiveProperty}));
    QDBusPendingReply<QVariant> reply = m_bus.asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *self) {
            QDBusPendingReply<QVariant> reply = *self;
            self->deleteLater();
            if (!reply.isValid()) {
                qCDebug(KWIN_CORE) << "Failed to get Active Property of " << m_sessionControllerName << " session:" << reply.error().message();
                return;
            }
            const bool active = reply.value().toBool();
            if (m_sessionActive != active) {
                m_sessionActive = active;
                emit sessionActiveChanged(m_sessionActive);
            }
        }
    );
}

void LogindIntegration::getVirtualTerminal()
{
    if (!m_connected || m_sessionPath.isEmpty()) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionPath,
                                                          s_dbusPropertiesInterface,
                                                          QStringLiteral("Get"));
    message.setArguments(QVariantList({m_sessionControllerSessionInterface, QStringLiteral("VTNr")}));
    QDBusPendingReply<QVariant> reply = m_bus.asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *self) {
            QDBusPendingReply<QVariant> reply = *self;
            self->deleteLater();
            if (!reply.isValid()) {
                qCDebug(KWIN_CORE) << "Failed to get VTNr Property of " << m_sessionControllerName << " session:" << reply.error().message();
                return;
            }
            const int vt = reply.value().toUInt();
            if (m_vt != (int)vt) {
                m_vt = vt;
                emit virtualTerminalChanged(m_vt);
            }
        }
    );
}

void LogindIntegration::takeControl()
{
    if (!m_connected || m_sessionPath.isEmpty() || m_sessionControl) {
        return;
    }
    static bool s_recursionCheck = false;
    if (s_recursionCheck) {
        return;
    }
    s_recursionCheck = true;

    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionPath,
                                                          m_sessionControllerSessionInterface,
                                                          QStringLiteral("TakeControl"));
    message.setArguments(QVariantList({QVariant(false)}));
    QDBusPendingReply<void> session = m_bus.asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(session, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *self) {
            s_recursionCheck = false;
            QDBusPendingReply<void> reply = *self;
            self->deleteLater();
            if (!reply.isValid()) {
                qCDebug(KWIN_CORE) << "Failed to get session control" << reply.error().message();
                emit hasSessionControlChanged(false);
                return;
            }
            qCDebug(KWIN_CORE) << "Gained session control";
            m_sessionControl = true;
            emit hasSessionControlChanged(true);
            m_bus.connect(m_sessionControllerService, m_sessionPath,
                          m_sessionControllerSessionInterface, QStringLiteral("PauseDevice"),
                          this, SLOT(pauseDevice(uint,uint,QString)));
        }
    );
}

void LogindIntegration::releaseControl()
{
    if (!m_connected || m_sessionPath.isEmpty() || !m_sessionControl) {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionPath,
                                                          m_sessionControllerSessionInterface,
                                                          QStringLiteral("ReleaseControl"));
    m_bus.asyncCall(message);
    m_sessionControl = false;
    emit hasSessionControlChanged(false);
}

int LogindIntegration::takeDevice(const char *path)
{
    struct stat st;
    if (stat(path, &st) < 0) {
        qCDebug(KWIN_CORE) << "Could not stat the path";
        return -1;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionPath,
                                                          m_sessionControllerSessionInterface,
                                                          QStringLiteral("TakeDevice"));
    message.setArguments(QVariantList({QVariant(major(st.st_rdev)), QVariant(minor(st.st_rdev))}));
    // intended to be a blocking call
    QDBusMessage reply = m_bus.call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCDebug(KWIN_CORE) << "Could not take device" << path << ", cause: " << reply.errorMessage();
        return -1;
    }

    // The dup syscall removes the CLOEXEC flag as a side-effect. So use fcntl's F_DUPFD_CLOEXEC cmd.
    return fcntl(reply.arguments().first().value<QDBusUnixFileDescriptor>().fileDescriptor(), F_DUPFD_CLOEXEC, 0);
}

void LogindIntegration::releaseDevice(int fd)
{
    struct stat st;
    if (fstat(fd, &st) < 0) {
        qCDebug(KWIN_CORE) << "Could not stat the file descriptor";
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionPath,
                                                          m_sessionControllerSessionInterface,
                                                          QStringLiteral("ReleaseDevice"));
    message.setArguments(QVariantList({QVariant(major(st.st_rdev)), QVariant(minor(st.st_rdev))}));
    m_bus.asyncCall(message);
}

void LogindIntegration::pauseDevice(uint devMajor, uint devMinor, const QString &type)
{
    if (QString::compare(type, QStringLiteral("pause"), Qt::CaseInsensitive) == 0) {
        // unconditionally call complete
        QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService, m_sessionPath, m_sessionControllerSessionInterface, QStringLiteral("PauseDeviceComplete"));
        message.setArguments(QVariantList({QVariant(devMajor), QVariant(devMinor)}));
        m_bus.asyncCall(message);
    }
}

void LogindIntegration::getSeat()
{
    if (m_sessionPath.isEmpty()) {
        return;
    }
    qDBusRegisterMetaType<DBusLogindSeat>();
    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_sessionPath,
                                                          s_dbusPropertiesInterface,
                                                          QStringLiteral("Get"));
    message.setArguments(QVariantList({m_sessionControllerSessionInterface, QStringLiteral("Seat")}));
    message.setArguments(QVariantList({m_sessionControllerSessionInterface, QStringLiteral("Seat")}));
    QDBusPendingReply<QVariant> reply = m_bus.asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *self) {
            QDBusPendingReply<QVariant> reply = *self;
            self->deleteLater();
            if (!reply.isValid()) {
                qCDebug(KWIN_CORE) << "Failed to get Seat Property of " << m_sessionControllerName << " session:" << reply.error().message();
                return;
            }
            DBusLogindSeat seat = qdbus_cast<DBusLogindSeat>(reply.value().value<QDBusArgument>());
            const QString seatPath = seat.path.path();
            qCDebug(KWIN_CORE) << m_sessionControllerName << " seat:" << seat.name << "/" << seatPath;
            qCDebug(KWIN_CORE) << m_sessionControllerName << " seat:" << seat.name << "/" << seatPath;
            if (m_seatPath != seatPath) {
                m_seatPath = seatPath;
            }
            if (m_seatName != seat.name) {
                m_seatName = seat.name;
            }
        }
    );
}

void LogindIntegration::switchVirtualTerminal(quint32 vtNr)
{
    if (!m_connected || m_seatPath.isEmpty()) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(m_sessionControllerService,
                                                          m_seatPath,
                                                          m_sessionControllerSeatInterface,
                                                          QStringLiteral("SwitchTo"));
    message.setArguments(QVariantList{vtNr});
    m_bus.asyncCall(message);
}

} // namespace

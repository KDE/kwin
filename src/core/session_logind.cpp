/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session_logind.h"
#include "utils/common.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusUnixFileDescriptor>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if __has_include(<sys/sysmacros.h>)
#include <sys/sysmacros.h>
#endif

struct DBusLogindSeat
{
    QString id;
    QDBusObjectPath path;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusLogindSeat &seat)
{
    argument.beginStructure();
    argument << seat.id << seat.path;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusLogindSeat &seat)
{
    argument.beginStructure();
    argument >> seat.id >> seat.path;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(DBusLogindSeat)

namespace KWin
{

static const QString s_serviceName = QStringLiteral("org.freedesktop.login1");
static const QString s_propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
static const QString s_sessionInterface = QStringLiteral("org.freedesktop.login1.Session");
static const QString s_seatInterface = QStringLiteral("org.freedesktop.login1.Seat");
static const QString s_managerInterface = QStringLiteral("org.freedesktop.login1.Manager");
static const QString s_managerPath = QStringLiteral("/org/freedesktop/login1");

static QString findProcessSessionPath()
{
    const QString sessionId = qEnvironmentVariable("XDG_SESSION_ID", QStringLiteral("auto"));
    QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, s_managerPath,
                                                          s_managerInterface,
                                                          QStringLiteral("GetSession"));
    message.setArguments({sessionId});
    const QDBusMessage reply = QDBusConnection::systemBus().call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return QString();
    }

    return reply.arguments().constFirst().value<QDBusObjectPath>().path();
}

static bool takeControl(const QString &sessionPath)
{
    QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, sessionPath,
                                                          s_sessionInterface,
                                                          QStringLiteral("TakeControl"));
    message.setArguments({false});

    const QDBusMessage reply = QDBusConnection::systemBus().call(message);

    return reply.type() != QDBusMessage::ErrorMessage;
}

static void releaseControl(const QString &sessionPath)
{
    const QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, sessionPath,
                                                                s_sessionInterface,
                                                                QStringLiteral("ReleaseControl"));

    QDBusConnection::systemBus().asyncCall(message);
}

static bool activate(const QString &sessionPath)
{
    const QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, sessionPath,
                                                                s_sessionInterface,
                                                                QStringLiteral("Activate"));

    const QDBusMessage reply = QDBusConnection::systemBus().call(message);

    return reply.type() != QDBusMessage::ErrorMessage;
}

std::unique_ptr<LogindSession> LogindSession::create()
{
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(s_serviceName)) {
        return nullptr;
    }

    const QString sessionPath = findProcessSessionPath();
    if (sessionPath.isEmpty()) {
        qCWarning(KWIN_CORE) << "Could not determine the active graphical session";
        return nullptr;
    }

    if (!activate(sessionPath)) {
        qCWarning(KWIN_CORE, "Failed to activate %s session. Maybe another compositor is running?",
                  qPrintable(sessionPath));
        return nullptr;
    }

    if (!takeControl(sessionPath)) {
        qCWarning(KWIN_CORE, "Failed to take control of %s session. Maybe another compositor is running?",
                  qPrintable(sessionPath));
        return nullptr;
    }

    std::unique_ptr<LogindSession> session{new LogindSession(sessionPath)};
    if (session->initialize()) {
        return session;
    } else {
        return nullptr;
    }
}

bool LogindSession::isActive() const
{
    return m_isActive;
}

LogindSession::Capabilities LogindSession::capabilities() const
{
    return Capability::SwitchTerminal;
}

QString LogindSession::seat() const
{
    return m_seatId;
}

uint LogindSession::terminal() const
{
    return m_terminal;
}

int LogindSession::openRestricted(const QString &fileName)
{
    struct stat st;
    if (stat(fileName.toUtf8(), &st) < 0) {
        return -1;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, m_sessionPath,
                                                          s_sessionInterface,
                                                          QStringLiteral("TakeDevice"));
    // major() and minor() macros return ints on FreeBSD instead of uints.
    message.setArguments({uint(major(st.st_rdev)), uint(minor(st.st_rdev))});

    const QDBusMessage reply = QDBusConnection::systemBus().call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(KWIN_CORE, "Failed to open %s device (%s)",
                  qPrintable(fileName), qPrintable(reply.errorMessage()));
        return -1;
    }

    const QDBusUnixFileDescriptor descriptor = reply.arguments().constFirst().value<QDBusUnixFileDescriptor>();
    if (!descriptor.isValid()) {
        qCWarning(KWIN_CORE, "File descriptor for %s from logind is invalid", qPrintable(fileName));
        return -1;
    }

    return fcntl(descriptor.fileDescriptor(), F_DUPFD_CLOEXEC, 0);
}

void LogindSession::closeRestricted(int fileDescriptor)
{
    struct stat st;
    if (fstat(fileDescriptor, &st) < 0) {
        close(fileDescriptor);
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, m_sessionPath,
                                                          s_sessionInterface,
                                                          QStringLiteral("ReleaseDevice"));
    // major() and minor() macros return ints on FreeBSD instead of uints.
    message.setArguments({uint(major(st.st_rdev)), uint(minor(st.st_rdev))});

    QDBusConnection::systemBus().asyncCall(message);

    close(fileDescriptor);
}

void LogindSession::switchTo(uint terminal)
{
    QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, m_seatPath,
                                                          s_seatInterface,
                                                          QStringLiteral("SwitchTo"));
    message.setArguments({terminal});

    QDBusConnection::systemBus().asyncCall(message);
}

LogindSession::LogindSession(const QString &sessionPath)
    : m_sessionPath(sessionPath)
{
    qDBusRegisterMetaType<DBusLogindSeat>();
}

LogindSession::~LogindSession()
{
    releaseControl(m_sessionPath);
}

bool LogindSession::initialize()
{
    QDBusMessage activeMessage = QDBusMessage::createMethodCall(s_serviceName, m_sessionPath,
                                                                s_propertiesInterface,
                                                                QStringLiteral("Get"));
    activeMessage.setArguments({s_sessionInterface, QStringLiteral("Active")});

    QDBusMessage seatMessage = QDBusMessage::createMethodCall(s_serviceName, m_sessionPath,
                                                              s_propertiesInterface,
                                                              QStringLiteral("Get"));
    seatMessage.setArguments({s_sessionInterface, QStringLiteral("Seat")});

    QDBusMessage terminalMessage = QDBusMessage::createMethodCall(s_serviceName, m_sessionPath,
                                                                  s_propertiesInterface,
                                                                  QStringLiteral("Get"));
    terminalMessage.setArguments({s_sessionInterface, QStringLiteral("VTNr")});

    QDBusPendingReply<QVariant> activeReply =
        QDBusConnection::systemBus().asyncCall(activeMessage);
    QDBusPendingReply<QVariant> terminalReply =
        QDBusConnection::systemBus().asyncCall(terminalMessage);
    QDBusPendingReply<QVariant> seatReply =
        QDBusConnection::systemBus().asyncCall(seatMessage);

    // We must wait until all replies have been received because the drm backend needs a
    // valid seat name to properly select gpu devices, this also simplifies startup code.
    activeReply.waitForFinished();
    terminalReply.waitForFinished();
    seatReply.waitForFinished();

    if (activeReply.isError()) {
        qCWarning(KWIN_CORE) << "Failed to query Active session property:" << activeReply.error();
        return false;
    }
    if (terminalReply.isError()) {
        qCWarning(KWIN_CORE) << "Failed to query VTNr session property:" << terminalReply.error();
        return false;
    }
    if (seatReply.isError()) {
        qCWarning(KWIN_CORE) << "Failed to query Seat session property:" << seatReply.error();
        return false;
    }

    m_isActive = activeReply.value().toBool();
    m_terminal = terminalReply.value().toUInt();

    const DBusLogindSeat seat = qdbus_cast<DBusLogindSeat>(seatReply.value().value<QDBusArgument>());
    m_seatId = seat.id;
    m_seatPath = seat.path.path();

    QDBusConnection::systemBus().connect(s_serviceName, s_managerPath, s_managerInterface,
                                         QStringLiteral("PrepareForSleep"),
                                         this,
                                         SLOT(handlePrepareForSleep(bool)));

    QDBusConnection::systemBus().connect(s_serviceName, m_sessionPath, s_sessionInterface,
                                         QStringLiteral("PauseDevice"),
                                         this,
                                         SLOT(handlePauseDevice(uint, uint, QString)));

    QDBusConnection::systemBus().connect(s_serviceName, m_sessionPath, s_sessionInterface,
                                         QStringLiteral("ResumeDevice"),
                                         this,
                                         SLOT(handleResumeDevice(uint, uint, QDBusUnixFileDescriptor)));

    QDBusConnection::systemBus().connect(s_serviceName, m_sessionPath, s_propertiesInterface,
                                         QStringLiteral("PropertiesChanged"),
                                         this,
                                         SLOT(handlePropertiesChanged(QString, QVariantMap)));

    return true;
}

void LogindSession::updateActive(bool active)
{
    if (m_isActive != active) {
        m_isActive = active;
        Q_EMIT activeChanged(active);
    }
}

void LogindSession::handlePauseDevice(uint major, uint minor, const QString &type)
{
    Q_EMIT devicePaused(makedev(major, minor));

    if (type == QLatin1String("pause")) {
        QDBusMessage message = QDBusMessage::createMethodCall(s_serviceName, m_sessionPath,
                                                              s_sessionInterface,
                                                              QStringLiteral("PauseDeviceComplete"));
        message.setArguments({major, minor});

        QDBusConnection::systemBus().asyncCall(message);
    }
}

void LogindSession::handleResumeDevice(uint major, uint minor, QDBusUnixFileDescriptor fileDescriptor)
{
    // We don't care about the file descriptor as the libinput backend will re-open input devices
    // and the drm file descriptors remain valid after pausing gpus.

    Q_EMIT deviceResumed(makedev(major, minor));
}

void LogindSession::handlePropertiesChanged(const QString &interfaceName, const QVariantMap &properties)
{
    if (interfaceName == s_sessionInterface) {
        const QVariant active = properties.value(QStringLiteral("Active"));
        if (active.isValid()) {
            updateActive(active.toBool());
        }
    }
}

void LogindSession::handlePrepareForSleep(bool sleep)
{
    if (!sleep) {
        Q_EMIT awoke();
    }
}

} // namespace KWin

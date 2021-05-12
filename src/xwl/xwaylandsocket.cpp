/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xwaylandsocket.h"
#include "xwayland_logging.h"

#include <QCoreApplication>
#include <QFile>

#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace KWin
{

class UnixSocketAddress
{
public:
    enum class Type {
        Unix,
        Abstract,
    };

    UnixSocketAddress(const QString &socketPath, Type type);

    const sockaddr *data() const;
    int size() const;

private:
    QByteArray m_buffer;
};

UnixSocketAddress::UnixSocketAddress(const QString &socketPath, Type type)
{
    const QByteArray encodedSocketPath = QFile::encodeName(socketPath);

    int byteCount = offsetof(sockaddr_un, sun_path) + encodedSocketPath.size() + 1;
    if (type == Type::Abstract) {
        byteCount++; // For the first '\0'.
    }
    m_buffer.resize(byteCount);

    sockaddr_un *address = reinterpret_cast<sockaddr_un *>(m_buffer.data());
    address->sun_family = AF_UNIX;

    if (type == Type::Unix) {
        qstrcpy(address->sun_path, encodedSocketPath);
    } else {
        *address->sun_path = '\0';
        qstrcpy(address->sun_path + 1, encodedSocketPath);
    }
}

const sockaddr *UnixSocketAddress::data() const
{
    return reinterpret_cast<const sockaddr *>(m_buffer.data());
}

int UnixSocketAddress::size() const
{
    return m_buffer.size();
}

static QString lockFileNameForDisplay(int display)
{
    return QStringLiteral("/tmp/.X%1-lock").arg(display);
}

static QString socketFileNameForDisplay(int display)
{
    return QStringLiteral("/tmp/.X11-unix/X%1").arg(display);
}

static bool tryLockFile(const QString &lockFileName)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        QFile lockFile(lockFileName);
        if (lockFile.open(QFile::WriteOnly | QFile::NewOnly)) {
            char buffer[12];
            snprintf(buffer, sizeof(buffer), "%10lld\n", QCoreApplication::applicationPid());
            if (lockFile.write(buffer, sizeof(buffer) - 1) != sizeof(buffer) - 1) {
                qCWarning(KWIN_XWL) << "Failed to write pid to lock file:" << lockFile.errorString();
                lockFile.remove();
                return false;
            }
            return true;
        } else if (lockFile.open(QFile::ReadOnly)) {
            const int lockPid = lockFile.readLine().trimmed().toInt();
            if (!lockPid) {
                return false;
            }
            if (kill(lockPid, 0) < 0 && errno == ESRCH) {
                lockFile.remove(); // Try to grab the lock file in the next loop iteration.
            } else {
                return false;
            }
        }
    }
    return false;
}

static int listen_helper(const QString &filePath, UnixSocketAddress::Type type)
{
    const UnixSocketAddress socketAddress(filePath, type);

    int fileDescriptor = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fileDescriptor == -1) {
        return -1;
    }

    if (bind(fileDescriptor, socketAddress.data(), socketAddress.size()) == -1) {
        close(fileDescriptor);
        return -1;
    }

    if (listen(fileDescriptor, 1) == -1) {
        close(fileDescriptor);
        return -1;
    }

    return fileDescriptor;
}

static bool checkSocketsDirectory()
{
    struct stat info;
    const char *path = "/tmp/.X11-unix";

    if (lstat(path, &info) != 0) {
        if (errno == ENOENT) {
            qCWarning(KWIN_XWL) << path << "does not exist. Please check your installation";
            return false;
        }

        qCWarning(KWIN_XWL, "Failed to stat %s: %s", path, strerror(errno));
        return false;
    }

    if (!S_ISDIR(info.st_mode)) {
        qCWarning(KWIN_XWL) << path << "is not a directory. Broken system?";
        return false;
    }
    if (info.st_uid != 0 && info.st_uid != getuid()) {
        qCWarning(KWIN_XWL) << path << "is not owned by root or us";
        return false;
    }
    if (!(info.st_mode & S_ISVTX)) {
        qCWarning(KWIN_XWL) << path << "has no sticky bit on. Your system might be compromised!";
        return false;
    }

    return true;
}

XwaylandSocket::XwaylandSocket()
{
    if (!checkSocketsDirectory()) {
        return;
    }

    for (int display = 0; display < 100; ++display) {
        const QString socketFilePath = socketFileNameForDisplay(display);
        const QString lockFilePath = lockFileNameForDisplay(display);

        if (!tryLockFile(lockFilePath)) {
            continue;
        }

        const int unixFileDescriptor = listen_helper(socketFilePath, UnixSocketAddress::Type::Unix);
        if (unixFileDescriptor == -1) {
            QFile::remove(lockFilePath);
            continue;
        }

        const int abstractFileDescriptor = listen_helper(socketFilePath, UnixSocketAddress::Type::Abstract);
        if (abstractFileDescriptor == -1) {
            QFile::remove(lockFilePath);
            QFile::remove(socketFilePath);
            close(unixFileDescriptor);
            continue;
        }

        m_socketFilePath = socketFilePath;
        m_lockFilePath = lockFilePath;
        m_unixFileDescriptor = unixFileDescriptor;
        m_abstractFileDescriptor = abstractFileDescriptor;
        m_display = display;
        return;
    }

    qCWarning(KWIN_XWL) << "Failed to find free X11 connection socket";
}

XwaylandSocket::~XwaylandSocket()
{
    if (m_unixFileDescriptor != -1) {
        close(m_unixFileDescriptor);
    }
    if (m_abstractFileDescriptor != -1) {
        close(m_abstractFileDescriptor);
    }
    if (!m_socketFilePath.isEmpty()) {
        QFile::remove(m_socketFilePath);
    }
    if (!m_lockFilePath.isEmpty()) {
        QFile::remove(m_lockFilePath);
    }
}

bool XwaylandSocket::isValid() const
{
    return m_display != -1;
}

int XwaylandSocket::unixFileDescriptor() const
{
    return m_unixFileDescriptor;
}

int XwaylandSocket::abstractFileDescriptor() const
{
    return m_abstractFileDescriptor;
}

int XwaylandSocket::display() const
{
    return m_display;
}

QString XwaylandSocket::name() const
{
    return ":" + QString::number(m_display);
}

} // namespace KWin

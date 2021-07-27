/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * This tiny executable creates a socket, then starts kwin passing it the FD to the wayland socket.
 * The WAYLAND_DISPLAY environment variable gets set here and passed to all spawned kwin instances.
 * On any non-zero kwin exit kwin gets restarted.
 *
 * After restart kwin is relaunched but now with the KWIN_RESTART_COUNT env set to an incrementing counter
 *
 * It's somewhat  akin to systemd socket activation, but we also need the lock file, finding the next free socket
 * and so on, hence our own binary.
 *
 * Usage kwin_wayland_wrapper [argForKwin] [argForKwin] ...
 */

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>

#include "wl-socket.h"

#define WAYLAND_ENV_NAME "WAYLAND_DISPLAY"

class KWinWrapper : public QObject
{
    Q_OBJECT
public:
    KWinWrapper(QObject *parent);
    ~KWinWrapper();
    void run();
    int runKwin();

private:
    wl_socket *m_socket;
    QString m_oldWaylandEnv;
};

KWinWrapper::KWinWrapper(QObject *parent)
    : QObject(parent)
{
    m_socket = wl_socket_create();
    if (!m_socket) {
        qFatal("Could not create wayland socket");
    }

    // copy the old WAYLAND_DISPLAY as we are about to overwrite it and kwin may need it
    if (qEnvironmentVariableIsSet(WAYLAND_ENV_NAME)) {
        m_oldWaylandEnv = qgetenv(WAYLAND_ENV_NAME);
    }

    qputenv(WAYLAND_ENV_NAME, wl_socket_get_display_name(m_socket));
}

KWinWrapper::~KWinWrapper()
{
    wl_socket_destroy(m_socket);
}

void KWinWrapper::run()
{
    int crashCount = 0;

    while (crashCount < 10) {
        if (crashCount > 0) {
            qputenv("KWIN_RESTART_COUNT", QByteArray::number(crashCount));
        }

        int exitStatus = runKwin();

        if (exitStatus == 133) {
            crashCount = 1;
            qDebug() << "Compositor restarted, respawning";
        } else if (exitStatus == -1) {
            // kwin_crashed, lets go again
            qWarning() << "Compositor crashed, respawning";
        } else {
            qWarning() << "Compositor exited with code: " << exitStatus;
            break;
        }
    }
}

int KWinWrapper::runKwin()
{
    qDebug() << "Launching kwin";

    auto process = new QProcess(qApp);
    process->setProgram("kwin_wayland");

    QStringList args;
    args << "--wayland_fd" << QString::number(wl_socket_get_fd(m_socket));

    if (!m_oldWaylandEnv.isEmpty()) {
        args << "--wayland-display" << m_oldWaylandEnv;
    }
    // attach our main process arguments
    // the first entry is dropped as it will be our program name
    args << qApp->arguments().mid(1);

    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->setArguments(args);
    process->start();
    process->waitForFinished(-1);

    if (process->exitStatus() == QProcess::CrashExit) {
        return -1;
    }

    return process->exitCode();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    KWinWrapper wrapper(&app);
    wrapper.run();

    return 0;
}

#include "kwin_wrapper.moc"

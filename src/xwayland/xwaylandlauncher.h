/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/filedescriptor.h"

#include <QList>
#include <QObject>
#include <QProcess>
#include <QSocketNotifier>
#include <QTemporaryFile>
#include <memory>

#include <kwin_export.h>

class QTimer;

namespace KWin
{
class XwaylandSocket;

namespace Xwl
{

class KWIN_EXPORT XwaylandLauncher : public QObject
{
    Q_OBJECT
public:
    explicit XwaylandLauncher(QObject *parent);
    ~XwaylandLauncher();

    /**
     * Set file descriptors that xwayland should use for listening
     * This is to be used in conjuction with kwin_wayland_wrapper which creates a socket externally
     * That external process is responsible for setting up the DISPLAY env with a valid value.
     * Ownership of the file descriptor is not transferrred.
     */
    void setListenFDs(const QList<int> &listenFds);

    /**
     * Sets the display name used by XWayland (i.e ':0')
     * This is to be used in conjuction with kwin_wayland_wrapper to provide the name of the socket
     * created externally
     */
    void setDisplayName(const QString &displayName);

    /**
     * Sets the xauthority file to be used by XWayland
     * This is to be used in conjuction with kwin_wayland_wrapper
     */
    void setXauthority(const QString &xauthority);

    void enable();
    void disable();
    bool start();
    void stop();

    QString displayName() const;
    QString xauthority() const;
    FileDescriptor takeXcbConnectionFd();

    /**
     * @internal
     */
    QProcess *process() const;
Q_SIGNALS:
    /**
     * This signal is emitted when the Xwayland server is ready to accept connections.
     */
    void ready();
    /**
     * This signal is emitted when the Xwayland server is started.
     */
    void started();
    /**
     * This signal is emitted when the Xwayland server quits or crashes
     */
    void finished();
    /**
     * This signal is emitted when an error occurs with the Xwayland server.
     */
    void errorOccurred();

private Q_SLOTS:
    void resetCrashCount();
    void handleXwaylandFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleXwaylandError(QProcess::ProcessError error);

private:
    QProcess *m_xwaylandProcess = nullptr;
    std::unique_ptr<QSocketNotifier> m_readyNotifier;
    QTimer *m_resetCrashCountTimer = nullptr;
    // this is only used when kwin is run without kwin_wayland_wrapper
    std::unique_ptr<XwaylandSocket> m_socket;
    QList<int> m_listenFds;
    QString m_displayName;
    QString m_xAuthority;

    bool m_enabled = false;
    int m_crashCount = 0;
    FileDescriptor m_readyFd;
    FileDescriptor m_xcbConnectionFd;
};

}
}

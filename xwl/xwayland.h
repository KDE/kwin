/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_XWAYLAND
#define KWIN_XWL_XWAYLAND

#include "xwayland_interface.h"

#include <QFutureWatcher>
#include <QProcess>
#include <QSocketNotifier>

namespace KWin
{
class ApplicationWaylandAbstract;

namespace Xwl
{

class Xwayland : public XwaylandInterface
{
    Q_OBJECT

public:
    Xwayland(ApplicationWaylandAbstract *app, QObject *parent = nullptr);
    ~Xwayland() override;

    /**
     * Returns the associated Xwayland process or @c null if the Xwayland server is inactive.
     */
    QProcess *process() const override;

public Q_SLOTS:
    /**
     * Starts the Xwayland server.
     *
     * This method will spawn an Xwayland process and will establish a new XCB connection to it.
     * If an error has occurred during the startup, the errorOccurred() signal is going to
     * be emitted. If the Xwayland server has started successfully, the started() signal will be
     * emitted.
     *
     * @see started(), stop()
     */
    void start();
    /**
     * Stops the Xwayland server.
     *
     * This method will destroy the existing XCB connection as well all connected X11 clients.
     *
     * A SIGTERM signal will be sent to the Xwayland process. If Xwayland doesn't shut down
     * within a reasonable amount of time (5 seconds), a SIGKILL signal will be sent and thus
     * the process will be killed for good.
     *
     * If the Xwayland process crashes, the server will be stopped automatically.
     *
     * @see start()
     */
    void stop();
    /**
     * Restarts the Xwayland server. This method is equivalent to calling stop() and start().
     */
    void restart();

Q_SIGNALS:
    /**
     * This signal is emitted when the Xwayland server has been started successfully and it is
     * ready to accept and manage X11 clients.
     */
    void started();
    /**
     * This signal is emitted when an error occurs with the Xwayland server.
     */
    void errorOccurred();

private Q_SLOTS:
    void dispatchEvents();
    void resetCrashCount();

    void handleXwaylandStarted();
    void handleXwaylandFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleXwaylandCrashed();
    void handleXwaylandError(QProcess::ProcessError error);
    void handleXwaylandReady();

private:
    void installSocketNotifier();
    void uninstallSocketNotifier();

    bool createX11Connection();
    void destroyX11Connection();

    DragEventReply dragMoveFilter(Toplevel *target, const QPoint &pos) override;

    int m_displayFileDescriptor = -1;
    int m_xcbConnectionFd = -1;
    QProcess *m_xwaylandProcess = nullptr;
    QSocketNotifier *m_socketNotifier = nullptr;
    QTimer *m_resetCrashCountTimer = nullptr;
    QByteArray m_displayName;
    QFutureWatcher<QByteArray> *m_watcher = nullptr;
    ApplicationWaylandAbstract *m_app;
    int m_crashCount = 0;

    Q_DISABLE_COPY(Xwayland)
};

} // namespace Xwl
} // namespace KWin

#endif

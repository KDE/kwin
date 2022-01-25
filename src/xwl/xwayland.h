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

#include <QProcess>
#include <QSocketNotifier>
#include <QTemporaryFile>

class KSelectionOwner;

namespace KWin
{
class AbstractOutput;
class ApplicationWaylandAbstract;
class XwaylandSocket;

namespace Xwl
{
class XrandrEventFilter;

class KWIN_EXPORT Xwayland : public XwaylandInterface
{
    Q_OBJECT

public:
    Xwayland(ApplicationWaylandAbstract *app, QObject *parent = nullptr);
    ~Xwayland() override;

    /**
     * Returns the associated Xwayland process or @c null if the Xwayland server is inactive.
     */
    QProcess *process() const override;

    /**
     * Set file descriptors that xwayland should use for listening
     * This is to be used in conjuction with kwin_wayland_wrapper which creates a socket externally
     * That external process is responsible for setting up the DISPLAY env with a valid value.
     * Ownership of the file descriptor is not transferrred.
     */
    void setListenFDs(const QVector<int> &listenFds);

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

    void handleXwaylandFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleXwaylandCrashed();
    void handleXwaylandError(QProcess::ProcessError error);
    void handleXwaylandReady();

    void handleSelectionLostOwnership();
    void handleSelectionFailedToClaimOwnership();
    void handleSelectionClaimedOwnership();

private:
    friend class XrandrEventFilter;

    void installSocketNotifier();
    void uninstallSocketNotifier();
    void maybeDestroyReadyNotifier();
    void updatePrimary(AbstractOutput *primaryOutput);

    bool startInternal();
    void stopInternal();
    void restartInternal();

    bool createX11Connection();
    void destroyX11Connection();

    DragEventReply dragMoveFilter(Toplevel *target, const QPoint &pos) override;
    KWaylandServer::AbstractDropHandler *xwlDropHandler() override;

    int m_xcbConnectionFd = -1;
    QProcess *m_xwaylandProcess = nullptr;
    QSocketNotifier *m_socketNotifier = nullptr;
    QSocketNotifier *m_readyNotifier = nullptr;
    QTimer *m_resetCrashCountTimer = nullptr;
    ApplicationWaylandAbstract *m_app;
    QScopedPointer<KSelectionOwner> m_selectionOwner;
    // this is only used when kwin is run without kwin_wayland_wrapper
    QScopedPointer<XwaylandSocket> m_socket;

    QVector<int> m_listenFds;
    QString m_displayName;
    QString m_xAuthority;

    int m_crashCount = 0;
    XrandrEventFilter *m_xrandrEventsFilter = nullptr;

    Q_DISABLE_COPY(Xwayland)
};

} // namespace Xwl
} // namespace KWin

#endif

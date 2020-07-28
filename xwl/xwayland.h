/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>
Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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
#ifndef KWIN_XWL_XWAYLAND
#define KWIN_XWL_XWAYLAND

#include "xwayland_interface.h"

#include <QProcess>
#include <QSocketNotifier>

#include <xcb/xproto.h>

namespace KWin
{
class ApplicationWaylandAbstract;

namespace Xwl
{
class DataBridge;

class Xwayland : public XwaylandInterface
{
    Q_OBJECT

public:
    static Xwayland *self();

    Xwayland(ApplicationWaylandAbstract *app, QObject *parent = nullptr);
    ~Xwayland() override;

    const xcb_query_extension_reply_t *xfixes() const {
        return m_xfixes;
    }

    /**
     * Returns the associated Xwayland process or @c null if the Xwayland server is inactive.
     */
    QProcess *process() const override;

public Q_SLOTS:
    /**
     * Starts the Xwayland server.
     *
     * This method will spawn an Xwayland process and will establish a new XCB connection to it.
     * If a fatal error has occurred during the startup, the criticalError() signal is going to
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
    void criticalError(int code);

private Q_SLOTS:
    void dispatchEvents();

    void handleXwaylandStarted();
    void handleXwaylandFinished(int exitCode);
    void handleXwaylandError(QProcess::ProcessError error);

private:
    void installSocketNotifier();
    void uninstallSocketNotifier();

    void createX11Connection();
    void destroyX11Connection();
    void continueStartupWithX();

    DragEventReply dragMoveFilter(Toplevel *target, const QPoint &pos) override;

    int m_displayFileDescriptor = -1;
    int m_xcbConnectionFd = -1;
    QProcess *m_xwaylandProcess = nullptr;
    const xcb_query_extension_reply_t *m_xfixes = nullptr;
    DataBridge *m_dataBridge = nullptr;
    QSocketNotifier *m_socketNotifier = nullptr;
    ApplicationWaylandAbstract *m_app;

    Q_DISABLE_COPY(Xwayland)
};

} // namespace Xwl
} // namespace KWin

#endif

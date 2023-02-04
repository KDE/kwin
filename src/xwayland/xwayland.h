/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include "xwayland_interface.h"

#include <memory>

class KSelectionOwner;
class QSocketNotifier;

namespace KWin
{
class Output;
class Application;

namespace Xwl
{
class XrandrEventFilter;
class XwaylandInputSpy;
class XwaylandLauncher;
class DataBridge;

class KWIN_EXPORT Xwayland : public QObject, public XwaylandInterface
{
    Q_OBJECT

public:
    Xwayland(Application *app);
    ~Xwayland() override;

    void start();

    XwaylandLauncher *xwaylandLauncher() const;

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
    void handleXwaylandFinished();
    void handleXwaylandReady();

    void handleSelectionLostOwnership();
    void handleSelectionFailedToClaimOwnership();
    void handleSelectionClaimedOwnership();

private:
    friend class XrandrEventFilter;

    enum class DispatchEventsMode {
        Poll,
        EventQueue
    };
    void dispatchEvents(DispatchEventsMode mode);

    void installSocketNotifier();
    void uninstallSocketNotifier();
    void updatePrimary();
    void refreshEavesdropping();

    bool createX11Connection();
    void destroyX11Connection();

    DragEventReply dragMoveFilter(Window *target, const QPoint &pos) override;
    KWaylandServer::AbstractDropHandler *xwlDropHandler() override;
    QSocketNotifier *m_socketNotifier = nullptr;

    Application *m_app;
    std::unique_ptr<KSelectionOwner> m_selectionOwner;
    std::unique_ptr<DataBridge> m_dataBridge;

    XrandrEventFilter *m_xrandrEventsFilter = nullptr;
    XwaylandLauncher *m_launcher;
    std::unique_ptr<XwaylandInputSpy> m_inputSpy;

    Q_DISABLE_COPY(Xwayland)
};

} // namespace Xwl
} // namespace KWin

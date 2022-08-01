/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_XWAYLAND
#define KWIN_XWL_XWAYLAND

#include <memory>

#include "xwayland_interface.h"

class KSelectionOwner;
class QSocketNotifier;

namespace KWin
{
class Output;
class Application;

namespace Xwl
{
class XrandrEventFilter;
class XwaylandLauncher;

class KWIN_EXPORT Xwayland : public XwaylandInterface
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
    void dispatchEvents();

    void handleSelectionLostOwnership();
    void handleSelectionFailedToClaimOwnership();
    void handleSelectionClaimedOwnership();

private:
    friend class XrandrEventFilter;

    void installSocketNotifier();
    void uninstallSocketNotifier();
    void updatePrimary(Output *primaryOutput);

    bool createX11Connection();
    void destroyX11Connection();

    DragEventReply dragMoveFilter(Window *target, const QPoint &pos) override;
    KWaylandServer::AbstractDropHandler *xwlDropHandler() override;
    QSocketNotifier *m_socketNotifier = nullptr;

    Application *m_app;
    std::unique_ptr<KSelectionOwner> m_selectionOwner;

    XrandrEventFilter *m_xrandrEventsFilter = nullptr;
    XwaylandLauncher *m_launcher;

    Q_DISABLE_COPY(Xwayland)
};

} // namespace Xwl
} // namespace KWin

#endif

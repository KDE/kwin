/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "config-kwin.h"

#if !KWIN_BUILD_X11
#error Do not include on non-X11 builds
#endif

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
class XwaylandInputFilter;
class XwaylandLauncher;
class DataBridge;

class KWIN_EXPORT Xwayland : public QObject, public XwaylandInterface
{
    Q_OBJECT

public:
    Xwayland(Application *app);
    ~Xwayland() override;

    void init();

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

    void runXWaylandStartupScripts();

    DragEventReply dragMoveFilter(Window *target) override;
    AbstractDropHandler *xwlDropHandler() override;
    QSocketNotifier *m_socketNotifier = nullptr;

    Application *m_app;
    std::unique_ptr<KSelectionOwner> m_windowManagerSelectionOwner;
    std::unique_ptr<KSelectionOwner> m_compositingManagerSelectionOwner;
    std::unique_ptr<DataBridge> m_dataBridge;

    XrandrEventFilter *m_xrandrEventsFilter = nullptr;
    XwaylandLauncher *m_launcher;
    std::unique_ptr<XwaylandInputFilter> m_inputFilter;

    Q_DISABLE_COPY(Xwayland)
};

} // namespace Xwl
} // namespace KWin

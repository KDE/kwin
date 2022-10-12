/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWaylandServer
{
class XdgToplevelInterface;
class XdgPopupInterface;
class XdgShellInterface;
}

namespace KWin
{

class XdgShellIntegration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit XdgShellIntegration();

private:
    void registerXdgToplevel(KWaylandServer::XdgToplevelInterface *toplevel);
    void registerXdgPopup(KWaylandServer::XdgPopupInterface *popup);
    void createXdgToplevelWindow(KWaylandServer::XdgToplevelInterface *surface);

    std::unique_ptr<KWaylandServer::XdgShellInterface> m_shell;
};

} // namespace KWin

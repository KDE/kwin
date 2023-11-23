/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

#include <chrono>

namespace KWin
{

class XdgShellInterface;
class XdgToplevelInterface;
class XdgPopupInterface;

class XdgShellIntegration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit XdgShellIntegration(QObject *parent = nullptr);

    std::chrono::milliseconds pingTimeout() const;
    void setPingTimeout(std::chrono::milliseconds pingTimeout);

private:
    void registerXdgToplevel(XdgToplevelInterface *toplevel);
    void registerXdgPopup(XdgPopupInterface *popup);
    void createXdgToplevelWindow(XdgToplevelInterface *surface);

    XdgShellInterface *m_shell;
};

} // namespace KWin

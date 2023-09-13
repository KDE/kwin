/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWin
{

class XdgToplevelInterface;
class XdgPopupInterface;

class XdgShellIntegration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit XdgShellIntegration(QObject *parent = nullptr);

private:
    void registerXdgToplevel(XdgToplevelInterface *toplevel);
    void registerXdgPopup(XdgPopupInterface *popup);
    void createXdgToplevelWindow(XdgToplevelInterface *surface);
};

} // namespace KWin

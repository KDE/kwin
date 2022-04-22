/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "x11window.h"

namespace KWin
{

/**
 * The XwaylandClient class represents a managed Xwayland client.
 */
class XwaylandClient : public X11Window
{
    Q_OBJECT

public:
    explicit XwaylandClient();

    bool wantsSyncCounter() const override;

private:
    void associate();
    void initialize();
};

} // namespace KWin

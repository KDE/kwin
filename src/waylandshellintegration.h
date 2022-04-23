/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "window.h"

namespace KWin
{

class WaylandShellIntegration : public QObject
{
    Q_OBJECT

public:
    explicit WaylandShellIntegration(QObject *parent = nullptr);

Q_SIGNALS:
    void windowCreated(Window *window);
};

} // namespace KWin

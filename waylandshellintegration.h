/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "abstract_client.h"

namespace KWin
{

class WaylandShellIntegration : public QObject
{
    Q_OBJECT

public:
    explicit WaylandShellIntegration(QObject *parent = nullptr);

Q_SIGNALS:
    void clientCreated(AbstractClient *client);
};

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <kwaylandextras_interface_p.h>

class KWaylandExtrasKWinPlugin : public KWaylandExtrasPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KWaylandExtrasPluginInterface_iid FILE "kwaylandextras.json")
    Q_INTERFACES(KWaylandExtrasPluginInterface)

public:
    void requestXdgActivationToken(QWindow *win, uint32_t serial, const QString &app_id) override;
    quint32 lastInputSerial(QWindow *window) override;
};

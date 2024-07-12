/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <eiscontext.h>

namespace KWin
{
class XWaylandEisContext : public EisContext
{
public:
    XWaylandEisContext(EisBackend *backend);

    const QByteArray socketName;

private:
    void connectionRequested(eis_client *client) override;
};
}

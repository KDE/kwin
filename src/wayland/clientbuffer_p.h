/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbuffer.h"

namespace KWaylandServer
{
class ClientBufferPrivate
{
public:
    virtual ~ClientBufferPrivate()
    {
    }

    wl_resource *resource = nullptr;
};

} // namespace KWaylandServer

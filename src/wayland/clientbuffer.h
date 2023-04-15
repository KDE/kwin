/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "core/graphicsbuffer.h"

struct wl_resource;

namespace KWaylandServer
{

/**
 * The ClientBuffer class represents a client buffer.
 *
 * While the ClientBuffer is referenced, it won't be destroyed. Note that the client can
 * still destroy the wl_buffer object while the ClientBuffer is referenced by the compositor.
 * You can use the isDestroyed() function to check whether the wl_buffer object has been
 * destroyed.
 */
class KWIN_EXPORT ClientBuffer : public KWin::GraphicsBuffer
{
    Q_OBJECT

public:
    static ClientBuffer *get(wl_resource *resource);
};

} // namespace KWaylandServer

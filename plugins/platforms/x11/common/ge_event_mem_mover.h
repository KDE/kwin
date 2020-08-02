/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <xcb/xcb.h>

#include <cstring>

namespace KWin
{

class GeEventMemMover
{
public:
    GeEventMemMover(xcb_generic_event_t *event)
        : m_event(reinterpret_cast<xcb_ge_generic_event_t *>(event))
    {
        // xcb event structs contain stuff that wasn't on the wire, the full_sequence field
        // adds an extra 4 bytes and generic events cookie data is on the wire right after the standard 32 bytes.
        // Move this data back to have the same layout in memory as it was on the wire
        // and allow casting, overwriting the full_sequence field.
        memmove((char*) m_event + 32, (char*) m_event + 36, m_event->length * 4);
    }
    ~GeEventMemMover()
    {
        // move memory layout back, so that Qt can do the same without breaking
        memmove((char*) m_event + 36, (char *) m_event + 32, m_event->length * 4);
    }

    xcb_ge_generic_event_t *operator->() const {
        return m_event;
    }

private:
    xcb_ge_generic_event_t *m_event;
};

}

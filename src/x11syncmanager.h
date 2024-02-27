/*
    SPDX-FileCopyrightText: 2014 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opengl/glutils.h"

#include <xcb/sync.h>
#include <xcb/xcb.h>

namespace KWin
{

class RenderBackend;

/**
 * SyncObject represents a fence used to synchronize operations in the kwin command stream
 * with operations in the X command stream.
 */
class X11SyncObject
{
public:
    enum State {
        Ready,
        TriggerSent,
        Waiting,
        Done,
        Resetting,
    };

    X11SyncObject();
    ~X11SyncObject();

    State state() const
    {
        return m_state;
    }

    void trigger();
    void wait();
    bool finish();
    void reset();
    void finishResetting();

private:
    State m_state;
    GLsync m_sync;
    xcb_sync_fence_t m_fence;
    xcb_get_input_focus_cookie_t m_reset_cookie;
};

/**
 * SyncManager manages a set of fences used for explicit synchronization with the X command
 * stream.
 */
class X11SyncManager
{
public:
    enum {
        MaxFences = 4,
    };

    static X11SyncManager *create(RenderBackend *backend);
    ~X11SyncManager();

    bool endFrame();

    void triggerFence();
    void insertWait();

private:
    X11SyncManager();

    X11SyncObject *m_currentFence = nullptr;
    QList<X11SyncObject *> m_fences;
    int m_next = 0;
};

} // namespace KWin

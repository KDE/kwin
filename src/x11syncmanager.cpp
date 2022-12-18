/*
    SPDX-FileCopyrightText: 2014 Fredrik Höglund <fredrik@kde.org>

    Explicit command stream synchronization based on the sample implementation by James Jones <jajones@nvidia.com>,
    SPDX-FileCopyrightText: 2011 NVIDIA Corporation

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11syncmanager.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "main.h"
#include "scene/workspacescene.h"
#include "utils/common.h"

#include "kwinglplatform.h"

namespace KWin
{

X11SyncObject::X11SyncObject()
{
    m_state = Ready;

    xcb_connection_t *const connection = kwinApp()->x11Connection();
    m_fence = xcb_generate_id(connection);
    xcb_sync_create_fence(connection, kwinApp()->x11RootWindow(), m_fence, false);
    xcb_flush(connection);

    m_sync = glImportSyncEXT(GL_SYNC_X11_FENCE_EXT, m_fence, 0);
}

X11SyncObject::~X11SyncObject()
{
    xcb_connection_t *const connection = kwinApp()->x11Connection();
    // If glDeleteSync is called before the xcb fence is signalled
    // the nvidia driver (the only one to implement GL_SYNC_X11_FENCE_EXT)
    // deadlocks waiting for the fence to be signalled.
    // To avoid this, make sure the fence is signalled before
    // deleting the sync.
    if (m_state == Resetting || m_state == Ready) {
        trigger();
        // The flush is necessary!
        // The trigger command needs to be sent to the X server.
        xcb_flush(connection);
    }
    xcb_sync_destroy_fence(connection, m_fence);
    glDeleteSync(m_sync);

    if (m_state == Resetting) {
        xcb_discard_reply(connection, m_reset_cookie.sequence);
    }
}

void X11SyncObject::trigger()
{
    Q_ASSERT(m_state == Ready || m_state == Resetting);

    // Finish resetting the fence if necessary
    if (m_state == Resetting) {
        finishResetting();
    }

    xcb_sync_trigger_fence(kwinApp()->x11Connection(), m_fence);
    m_state = TriggerSent;
}

void X11SyncObject::wait()
{
    if (m_state != TriggerSent) {
        return;
    }

    glWaitSync(m_sync, 0, GL_TIMEOUT_IGNORED);
    m_state = Waiting;
}

bool X11SyncObject::finish()
{
    if (m_state == Done) {
        return true;
    }

    // Note: It is possible that we never inserted a wait for the fence.
    //       This can happen if we ended up not rendering the damaged
    //       window because it is fully occluded.
    Q_ASSERT(m_state == TriggerSent || m_state == Waiting);

    // Check if the fence is signaled
    GLint value;
    glGetSynciv(m_sync, GL_SYNC_STATUS, 1, nullptr, &value);

    if (value != GL_SIGNALED) {
        qCDebug(KWIN_CORE) << "Waiting for X fence to finish";

        // Wait for the fence to become signaled with a one second timeout
        const GLenum result = glClientWaitSync(m_sync, 0, 1000000000);

        switch (result) {
        case GL_TIMEOUT_EXPIRED:
            qCWarning(KWIN_CORE) << "Timeout while waiting for X fence";
            return false;

        case GL_WAIT_FAILED:
            qCWarning(KWIN_CORE) << "glClientWaitSync() failed";
            return false;
        }
    }

    m_state = Done;
    return true;
}

void X11SyncObject::reset()
{
    Q_ASSERT(m_state == Done);

    xcb_connection_t *const connection = kwinApp()->x11Connection();

    // Send the reset request along with a sync request.
    // We use the cookie to ensure that the server has processed the reset
    // request before we trigger the fence and call glWaitSync().
    // Otherwise there is a race condition between the reset finishing and
    // the glWaitSync() call.
    xcb_sync_reset_fence(connection, m_fence);
    m_reset_cookie = xcb_get_input_focus(connection);
    xcb_flush(connection);

    m_state = Resetting;
}

void X11SyncObject::finishResetting()
{
    Q_ASSERT(m_state == Resetting);
    free(xcb_get_input_focus_reply(kwinApp()->x11Connection(), m_reset_cookie, nullptr));
    m_state = Ready;
}

X11SyncManager *X11SyncManager::create()
{
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
        return nullptr;
    }

    if (Compositor::self()->backend()->compositingType() != OpenGLCompositing) {
        return nullptr;
    }

    GLPlatform *glPlatform = GLPlatform::instance();
    const bool haveSyncObjects = glPlatform->isGLES()
        ? hasGLVersion(3, 0)
        : hasGLVersion(3, 2) || hasGLExtension("GL_ARB_sync");

    if (hasGLExtension("GL_EXT_x11_sync_object") && haveSyncObjects) {
        const QString useExplicitSync = qEnvironmentVariable("KWIN_EXPLICIT_SYNC");

        if (useExplicitSync != QLatin1String("0")) {
            qCDebug(KWIN_CORE) << "Initializing fences for synchronization with the X command stream";
            return new X11SyncManager;
        } else {
            qCDebug(KWIN_CORE) << "Explicit synchronization with the X command stream disabled by environment variable";
        }
    }
    return nullptr;
}

X11SyncManager::X11SyncManager()
{
    for (int i = 0; i < MaxFences; ++i) {
        m_fences.append(new X11SyncObject);
    }
}

X11SyncManager::~X11SyncManager()
{
    Compositor::self()->scene()->makeOpenGLContextCurrent();
    qDeleteAll(m_fences);
}

bool X11SyncManager::endFrame()
{
    if (!m_currentFence) {
        return true;
    }

    for (int i = 0; i < std::min<int>(2, m_fences.count() - 1); i++) {
        const int index = (m_next + i) % m_fences.count();
        X11SyncObject *fence = m_fences[index];

        switch (fence->state()) {
        case X11SyncObject::Ready:
            break;

        case X11SyncObject::TriggerSent:
        case X11SyncObject::Waiting:
            if (!fence->finish()) {
                return false;
            }
            fence->reset();
            break;

        // Should not happen in practice since we always reset the fence after finishing it
        case X11SyncObject::Done:
            fence->reset();
            break;

        case X11SyncObject::Resetting:
            fence->finishResetting();
            break;
        }
    }

    m_currentFence = nullptr;
    return true;
}

void X11SyncManager::triggerFence()
{
    m_currentFence = m_fences[m_next];
    m_next = (m_next + 1) % m_fences.count();
    m_currentFence->trigger();
}

void X11SyncManager::insertWait()
{
    if (m_currentFence && m_currentFence->state() != X11SyncObject::Waiting) {
        m_currentFence->wait();
    }
}

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglnativefence.h"

#include <unistd.h>

namespace KWin
{

#ifndef EGL_ANDROID_native_fence_sync
#define EGL_SYNC_NATIVE_FENCE_ANDROID 0x3144
#define EGL_NO_NATIVE_FENCE_FD_ANDROID -1
#endif // EGL_ANDROID_native_fence_sync

EGLNativeFence::EGLNativeFence(::EGLDisplay display)
    : EGLNativeFence(display, eglCreateSyncKHR(display, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr))
{
    if (m_sync != EGL_NO_SYNC_KHR) {
        // The native fence will get a valid sync file fd only after a flush.
        glFlush();
        m_fileDescriptor = FileDescriptor(eglDupNativeFenceFDANDROID(m_display, m_sync));
    }
}

EGLNativeFence::EGLNativeFence(::EGLDisplay display, EGLSyncKHR sync)
    : m_sync(sync)
    , m_display(display)
{
}

EGLNativeFence::~EGLNativeFence()
{
    m_fileDescriptor.reset();
    if (m_sync != EGL_NO_SYNC_KHR) {
        eglDestroySyncKHR(m_display, m_sync);
    }
}

bool EGLNativeFence::isValid() const
{
    return m_sync != EGL_NO_SYNC_KHR && m_fileDescriptor.isValid();
}

const FileDescriptor &EGLNativeFence::fileDescriptor() const
{
    return m_fileDescriptor;
}

bool EGLNativeFence::waitSync() const
{
    return eglWaitSync(m_display, m_sync, 0) == EGL_TRUE;
}

EGLNativeFence EGLNativeFence::importFence(::EGLDisplay display, FileDescriptor &&fd)
{
    EGLint attributes[] = {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd.take(),
        EGL_NONE};
    return EGLNativeFence(display, eglCreateSyncKHR(display, EGL_SYNC_NATIVE_FENCE_ANDROID, attributes));
}

} // namespace KWin

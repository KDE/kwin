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

EGLNativeFence::EGLNativeFence(EGLDisplay display)
    : m_display(display)
{
    m_sync = eglCreateSyncKHR(m_display, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
    if (m_sync != EGL_NO_SYNC_KHR) {
        // The native fence will get a valid sync file fd only after a flush.
        glFlush();
        m_fileDescriptor = eglDupNativeFenceFDANDROID(m_display, m_sync);
    }
}

EGLNativeFence::~EGLNativeFence()
{
    if (m_fileDescriptor != EGL_NO_NATIVE_FENCE_FD_ANDROID) {
        close(m_fileDescriptor);
    }
    if (m_sync != EGL_NO_SYNC_KHR) {
        eglDestroySyncKHR(m_display, m_sync);
    }
}

bool EGLNativeFence::isValid() const
{
    return m_sync != EGL_NO_SYNC_KHR && m_fileDescriptor != EGL_NO_NATIVE_FENCE_FD_ANDROID;
}

int EGLNativeFence::fileDescriptor() const
{
    return m_fileDescriptor;
}

} // namespace KWin

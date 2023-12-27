/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <epoxy/egl.h>

#include "kwin_export.h"
#include "utils/filedescriptor.h"

namespace KWin
{

class EglDisplay;

class KWIN_EXPORT EGLNativeFence
{
public:
    explicit EGLNativeFence(EglDisplay *display);
    explicit EGLNativeFence(EglDisplay *display, EGLSyncKHR sync);
    EGLNativeFence(EGLNativeFence &&) = delete;
    EGLNativeFence(const EGLNativeFence &) = delete;
    ~EGLNativeFence();

    bool isValid() const;
    const FileDescriptor &fileDescriptor() const;
    bool waitSync() const;

    static EGLNativeFence importFence(EglDisplay *display, FileDescriptor &&fd);

private:
    EGLSyncKHR m_sync = EGL_NO_SYNC_KHR;
    EglDisplay *m_display = nullptr;
    FileDescriptor m_fileDescriptor;
};

} // namespace KWin

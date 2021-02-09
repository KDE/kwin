/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

#include <epoxy/egl.h>

namespace KWin
{

class EGLNativeFence
{
public:
    explicit EGLNativeFence(EGLDisplay display);
    ~EGLNativeFence();

    bool isValid() const;
    int fileDescriptor() const;

private:
    EGLSyncKHR m_sync = EGL_NO_SYNC_KHR;
    EGLDisplay m_display = EGL_NO_DISPLAY;
    int m_fileDescriptor = -1;

    Q_DISABLE_COPY(EGLNativeFence)
};

} // namespace KWin

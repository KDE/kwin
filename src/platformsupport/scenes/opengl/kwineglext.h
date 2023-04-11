/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <EGL/eglext.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WAYLAND_BUFFER_WL 0x31D5
#define EGL_WAYLAND_PLANE_WL 0x31D6
#define EGL_TEXTURE_Y_U_V_WL 0x31D7
#define EGL_TEXTURE_Y_UV_WL 0x31D8
#define EGL_TEXTURE_Y_XUXV_WL 0x31D9
#define EGL_TEXTURE_EXTERNAL_WL 0x31DA
#define EGL_WAYLAND_Y_INVERTED_WL 0x31DB
#endif // EGL_WL_bind_wayland_display

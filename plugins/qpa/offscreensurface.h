/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <epoxy/egl.h>
#include "fixqopengl.h"
#include <fixx11h.h>

#include <qpa/qplatformoffscreensurface.h>

namespace KWin
{
namespace QPA
{

class OffscreenSurface : public QPlatformOffscreenSurface
{
public:
    explicit OffscreenSurface(QOffscreenSurface *surface);
    ~OffscreenSurface() override;

    QSurfaceFormat format() const override;
    bool isValid() const override;

    EGLSurface eglSurface() const;

private:
    QSurfaceFormat m_format;

    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
};

} // namespace QPA
} // namespace KWin

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "offscreensurface.h"
#include "core/outputbackend.h"
#include "eglhelpers.h"
#include "main.h"
#include "opengl/egldisplay.h"

#include <QOffscreenSurface>

namespace KWin
{
namespace QPA
{

OffscreenSurface::OffscreenSurface(QOffscreenSurface *surface)
    : QPlatformOffscreenSurface(surface)
    , m_format(surface->requestedFormat())
{
}

QSurfaceFormat OffscreenSurface::format() const
{
    return m_format;
}

bool OffscreenSurface::isValid() const
{
    return true;
}

} // namespace QPA
} // namespace KWin

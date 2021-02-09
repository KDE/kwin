/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <epoxy/egl.h>

#include <QSurfaceFormat>

namespace KWin
{
namespace QPA
{

bool isOpenGLES();

EGLConfig configFromFormat(EGLDisplay display, const QSurfaceFormat &surfaceFormat, EGLint surfaceType = 0);
QSurfaceFormat formatFromConfig(EGLDisplay display, EGLConfig config);

} // namespace QPA
} // namespace KWin

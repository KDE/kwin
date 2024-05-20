/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opengl/glshader.h"
#include <memory>

namespace KWin
{
class RenderViewport;
class GLTexture;
class GLFramebuffer;
class Output;

struct Snapshot
{
    std::shared_ptr<GLTexture> texture;
    std::shared_ptr<GLFramebuffer> framebuffer;
};

/**
 * Initialises the @p snapshot with the dimensions of @p output and issues a new scene render into them.
 */
KWIN_EXPORT bool renderOutput(Snapshot &snapshot, Output *output);

/**
 * To be called from Effect::paintScreen overrides, it renders the effect into @p current so that it can be operated on
 */
KWIN_EXPORT bool paintScreenInTexture(Snapshot &current, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen);

}

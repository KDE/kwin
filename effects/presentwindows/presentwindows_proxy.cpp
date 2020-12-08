/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "presentwindows_proxy.h"
#include "presentwindows.h"

namespace KWin
{

PresentWindowsEffectProxy::PresentWindowsEffectProxy(PresentWindowsEffect* effect)
    : m_effect(effect)
{
}

PresentWindowsEffectProxy::~PresentWindowsEffectProxy()
{
}

void PresentWindowsEffectProxy::calculateWindowTransformations(EffectWindowList windows, int screen,
        WindowMotionManager& manager)
{
    return m_effect->calculateWindowTransformations(windows, screen, manager, true);
}

void PresentWindowsEffectProxy::reCreateGrids()
{
    m_effect->reCreateGrids();
}

} // namespace

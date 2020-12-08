/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cube_proxy.h"
#include "cube.h"
#include "cube_inside.h"

namespace KWin
{

CubeEffectProxy::CubeEffectProxy(CubeEffect* effect)
    : m_effect(effect)
{
}

CubeEffectProxy::~CubeEffectProxy()
{
}

void CubeEffectProxy::registerCubeInsideEffect(CubeInsideEffect* effect)
{
    m_effect->registerCubeInsideEffect(effect);
}

void CubeEffectProxy::unregisterCubeInsideEffect(CubeInsideEffect* effect)
{
    m_effect->unregisterCubeInsideEffect(effect);
}

} // namespace

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_CUBE_PROXY_H
#define KWIN_CUBE_PROXY_H

namespace KWin
{

class CubeEffect;
class CubeInsideEffect;

class CubeEffectProxy
{
public:
    explicit CubeEffectProxy(CubeEffect* effect);
    ~CubeEffectProxy();

    void registerCubeInsideEffect(CubeInsideEffect* effect);
    void unregisterCubeInsideEffect(CubeInsideEffect* effect);

private:
    CubeEffect* m_effect;
};

} // namespace

#endif // KWIN_CUBE_PROXY_H

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_CUBE_INSIDE_H
#define KWIN_CUBE_INSIDE_H
#include <kwineffects.h>

namespace KWin
{

class CubeInsideEffect : public Effect
{
public:
    CubeInsideEffect() {}
    ~CubeInsideEffect() override {}

    virtual void paint() = 0;
    virtual void setActive(bool active) = 0;
};

} // namespace

#endif // KWIN_CUBE_INSIDE_H

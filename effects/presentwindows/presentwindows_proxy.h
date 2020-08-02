/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_PRESENTWINDOWS_PROXY_H
#define KWIN_PRESENTWINDOWS_PROXY_H
#include <kwineffects.h>

namespace KWin
{

class PresentWindowsEffect;

class PresentWindowsEffectProxy
{
public:
    explicit PresentWindowsEffectProxy(PresentWindowsEffect* effect);
    ~PresentWindowsEffectProxy();

    void calculateWindowTransformations(EffectWindowList windows, int screen, WindowMotionManager& manager);

    void reCreateGrids();

private:
    PresentWindowsEffect* m_effect;
};

} // namespace

#endif

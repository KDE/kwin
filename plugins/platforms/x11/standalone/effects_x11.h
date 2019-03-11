/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010, 2011, 2017 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_EFFECTS_X11_H
#define KWIN_EFFECTS_X11_H

#include "effects.h"
#include "xcbutils.h"

#include <memory.h>

namespace KWin
{
class EffectsMouseInterceptionX11Filter;

class EffectsHandlerImplX11 : public EffectsHandlerImpl
{
    Q_OBJECT
public:
    explicit EffectsHandlerImplX11(Compositor *compositor, Scene *scene);
    ~EffectsHandlerImplX11() override;

    void defineCursor(Qt::CursorShape shape) override;

protected:
    bool doGrabKeyboard() override;
    void doUngrabKeyboard() override;

    void doStartMouseInterception(Qt::CursorShape shape) override;
    void doStopMouseInterception() override;

    void doCheckInputWindowStacking() override;

private:
    Xcb::Window m_mouseInterceptionWindow;
    std::unique_ptr<EffectsMouseInterceptionX11Filter> m_x11MouseInterception;
};

}

#endif

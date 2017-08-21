/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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
#ifndef KWIN_NON_COMPOSITED_OUTLINE_H
#define KWIN_NON_COMPOSITED_OUTLINE_H
#include "outline.h"
#include "xcbutils.h"

namespace KWin
{

class NonCompositedOutlineVisual : public OutlineVisual
{
public:
    NonCompositedOutlineVisual(Outline *outline);
    virtual ~NonCompositedOutlineVisual();
    virtual void show();
    virtual void hide();

private:
    // TODO: variadic template arguments for adding method arguments
    template <typename T>
    void forEachWindow(T method);
    bool m_initialized;
    Xcb::Window m_topOutline;
    Xcb::Window m_rightOutline;
    Xcb::Window m_bottomOutline;
    Xcb::Window m_leftOutline;
};

template <typename T>
inline
void NonCompositedOutlineVisual::forEachWindow(T method)
{
    (m_topOutline.*method)();
    (m_rightOutline.*method)();
    (m_bottomOutline.*method)();
    (m_leftOutline.*method)();
}

}

#endif

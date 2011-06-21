/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_UNMANAGED_H
#define KWIN_UNMANAGED_H

#include <netwm.h>

#include "toplevel.h"

namespace KWin
{

class Unmanaged
    : public Toplevel
{
    Q_OBJECT
public:
    Unmanaged(Workspace *ws);
    bool windowEvent(XEvent* e);
    void release();
    bool track(Window w);
    static void deleteUnmanaged(Unmanaged* c, allowed_t);
    virtual int desktop() const;
    virtual QStringList activities() const;
    virtual QPoint clientPos() const;
    virtual QSize clientSize() const;
    virtual QRect transparentRect() const;
protected:
    virtual void debug(QDebug& stream) const;
    virtual bool shouldUnredirect() const;
private:
    virtual ~Unmanaged(); // use release()
    // handlers for X11 events
    void mapNotifyEvent(XMapEvent* e);
    void unmapNotifyEvent(XUnmapEvent*e);
    void configureNotifyEvent(XConfigureEvent* e);
};

} // namespace

#endif

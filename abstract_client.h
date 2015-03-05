/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_ABSTRACT_CLIENT_H
#define KWIN_ABSTRACT_CLIENT_H

#include "toplevel.h"

namespace KWin
{

namespace TabBox
{
class TabBoxClientImpl;
}

class AbstractClient : public Toplevel
{
    Q_OBJECT
public:
    virtual ~AbstractClient();

    virtual QWeakPointer<TabBox::TabBoxClientImpl> tabBoxClient() const = 0;
    virtual void updateMouseGrab();
    virtual QString caption(bool full = true, bool stripped = false) const = 0;
    virtual bool isMinimized() const = 0;
    virtual bool isCloseable() const = 0;
    virtual bool isFirstInTabBox() const = 0;
    // TODO: remove boolean trap
    virtual bool isShown(bool shaded_is_shown) const = 0;
    virtual bool wantsTabFocus() const = 0;
    virtual bool isFullScreen() const = 0;
    virtual const QIcon &icon() const = 0;
    virtual void cancelAutoRaise() = 0;
    virtual bool isTransient() const;
    /**
     * Returns true for "special" windows and false for windows which are "normal"
     * (normal=window which has a border, can be moved by the user, can be closed, etc.)
     * true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
     * false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
     */
    virtual bool isSpecialWindow() const = 0;

    // TODO: remove boolean trap
    static bool belongToSameApplication(const AbstractClient* c1, const AbstractClient* c2, bool active_hack = false);

public Q_SLOTS:
    virtual void closeWindow() = 0;

protected:
    AbstractClient();
    // TODO: remove boolean trap
    virtual bool belongsToSameApplication(const AbstractClient *other, bool active_hack) const = 0;
};

}

Q_DECLARE_METATYPE(KWin::AbstractClient*)
Q_DECLARE_METATYPE(QList<KWin::AbstractClient*>)

#endif

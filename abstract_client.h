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
#include "options.h"

namespace KWin
{

class TabGroup;
class WindowRules;

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
    virtual bool skipSwitcher() const = 0;
    // TODO: remove boolean trap
    virtual AbstractClient *findModal(bool allow_itself = false) = 0;
    virtual void cancelAutoRaise() = 0;
    virtual bool isTransient() const;
    /**
     * Returns true for "special" windows and false for windows which are "normal"
     * (normal=window which has a border, can be moved by the user, can be closed, etc.)
     * true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
     * false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
     */
    virtual bool isSpecialWindow() const = 0;
    virtual bool isActive() const = 0;
    virtual void setActive(bool) =0;
    virtual void sendToScreen(int screen) = 0;
    virtual const QKeySequence &shortcut() const  = 0;
    virtual void setShortcut(const QString &cut) = 0;
    virtual bool performMouseCommand(Options::MouseCommand, const QPoint &globalPos) = 0;
    virtual void setOnAllDesktops(bool set) = 0;
    virtual void setDesktop(int) = 0;
    virtual void minimize(bool avoid_animation = false) = 0;
    virtual void unminimize(bool avoid_animation = false)= 0;
    virtual void setFullScreen(bool set, bool user = true) = 0;
    virtual bool keepAbove() const = 0;
    virtual void setKeepAbove(bool) = 0;
    virtual bool keepBelow() const = 0;
    virtual void setKeepBelow(bool) = 0;
    virtual TabGroup *tabGroup() const;
    Q_INVOKABLE virtual bool untab(const QRect &toGeometry = QRect(), bool clientRemoved = false);
    virtual MaximizeMode maximizeMode() const = 0;
    virtual void maximize(MaximizeMode) = 0;
    virtual bool noBorder() const = 0;
    virtual void setNoBorder(bool set) = 0;
    virtual void blockActivityUpdates(bool b = true) = 0;
    virtual QPalette palette() const = 0;
    virtual bool isResizable() const = 0;
    virtual bool isMovable() const = 0;
    virtual bool isMovableAcrossScreens() const = 0;
    virtual bool isShade() const = 0; // True only for ShadeNormal
    virtual ShadeMode shadeMode() const = 0; // Prefer isShade()
    virtual void setShade(bool set) = 0;
    virtual void setShade(ShadeMode mode) = 0;
    virtual bool isShadeable() const = 0;
    virtual bool isMaximizable() const = 0;
    virtual bool isMinimizable() const = 0;
    virtual bool userCanSetFullScreen() const = 0;
    virtual bool userCanSetNoBorder() const = 0;
    virtual void setOnAllActivities(bool set) = 0;
    virtual const WindowRules* rules() const = 0;
    virtual void takeFocus() = 0;
    virtual bool wantsInput() const = 0;
    virtual void checkWorkspacePosition(QRect oldGeometry = QRect(), int oldDesktop = -2) = 0;

    virtual void growHorizontal();
    virtual void shrinkHorizontal();
    virtual void growVertical();
    virtual void shrinkVertical();

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

/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_BRIDGE_H
#define KWIN_BRIDGE_H

#include <kdecorationbridge.h>

namespace KWin
{

class Client;

class Bridge : public KDecorationBridgeUnstable
{
public:
    Bridge(Client* cl);
    virtual bool isActive() const;
    virtual bool isCloseable() const;
    virtual bool isMaximizable() const;
    virtual MaximizeMode maximizeMode() const;
    virtual bool isMinimizable() const;
    virtual bool providesContextHelp() const;
    virtual int desktop() const;
    virtual bool isModal() const;
    virtual bool isShadeable() const;
    virtual bool isShade() const;
    virtual bool isSetShade() const;
    virtual bool keepAbove() const;
    virtual bool keepBelow() const;
    virtual bool isMovable() const;
    virtual bool isResizable() const;
    virtual NET::WindowType windowType(unsigned long supported_types) const;
    virtual QIcon icon() const;
    virtual QString caption() const;
    virtual void processMousePressEvent(QMouseEvent*);
    virtual void showWindowMenu(const QPoint &);
    virtual void showWindowMenu(const QRect &);
    virtual void performWindowOperation(WindowOperation);
    virtual void setMask(const QRegion&, int);
    virtual bool isPreview() const;
    virtual QRect geometry() const;
    virtual QRect iconGeometry() const;
    virtual QRegion unobscuredRegion(const QRegion& r) const;
    virtual WId windowId() const;
    virtual void closeWindow();
    virtual void maximize(MaximizeMode mode);
    virtual void minimize();
    virtual void showContextHelp();
    virtual void setDesktop(int desktop);
    virtual void titlebarDblClickOperation();
    virtual void titlebarMouseWheelOperation(int delta);
    virtual void setShade(bool set);
    virtual void setKeepAbove(bool);
    virtual void setKeepBelow(bool);
    virtual int currentDesktop() const;
    virtual QWidget* initialParentWidget() const;
    virtual Qt::WFlags initialWFlags() const;
    virtual void grabXServer(bool grab);

    virtual bool compositingActive() const;
    virtual QRect transparentRect() const;

    // Window tabbing
    virtual QString caption(int idx) const;
    virtual void closeTab(long id);
    virtual void closeTabGroup();
    virtual long currentTabId() const;
    virtual QIcon icon(int idx) const;
    virtual void setCurrentTab(long id);
    virtual void showWindowMenu(const QPoint &, long id);
    virtual void tab_A_before_B(long A, long B);
    virtual void tab_A_behind_B(long A, long B);
    virtual int tabCount() const;
    virtual long tabId(int idx) const;
    virtual void untab(long id, const QRect& newGeom);

    virtual WindowOperation buttonToWindowOperation(Qt::MouseButtons button);

private:
    Client *clientForId(long id) const;
    static inline long tabIdOf(Client *c) {
        return reinterpret_cast<long>(c);
    }
    Client* c;
};

} // namespace

#endif

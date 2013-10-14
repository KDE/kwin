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

class Bridge : public KDecorationBridge
{
public:
    explicit Bridge(Client* cl);
    virtual bool isActive() const override;
    virtual bool isCloseable() const override;
    virtual bool isMaximizable() const override;
    virtual MaximizeMode maximizeMode() const override;
    virtual QuickTileMode quickTileMode() const override;
    virtual bool isMinimizable() const override;
    virtual bool providesContextHelp() const override;
    virtual int desktop() const override;
    virtual bool isModal() const override;
    virtual bool isShadeable() const override;
    virtual bool isShade() const override;
    virtual bool isSetShade() const override;
    virtual bool keepAbove() const override;
    virtual bool keepBelow() const override;
    virtual bool isMovable() const override;
    virtual bool isResizable() const override;
    virtual NET::WindowType windowType(unsigned long supported_types) const override;
    virtual QIcon icon() const override;
    virtual QString caption() const override;
    virtual void processMousePressEvent(QMouseEvent*) override;
    virtual void showWindowMenu(const QPoint &) override;
    virtual void showWindowMenu(const QRect &) override;
    virtual void showApplicationMenu(const QPoint &) override;
    virtual bool menuAvailable() const override;
    virtual void performWindowOperation(WindowOperation) override;
    virtual void setMask(const QRegion&, int) override;
    virtual bool isPreview() const override;
    virtual QRect geometry() const override;
    virtual QRect iconGeometry() const override;
    virtual QRegion unobscuredRegion(const QRegion& r) const override;
    virtual WId windowId() const override;
    virtual void closeWindow() override;
    virtual void maximize(MaximizeMode mode) override;
    virtual void minimize() override;
    virtual void showContextHelp() override;
    virtual void setDesktop(int desktop) override;
    virtual void titlebarDblClickOperation() override;
    virtual void titlebarMouseWheelOperation(int delta) override;
    virtual void setShade(bool set) override;
    virtual void setKeepAbove(bool) override;
    virtual void setKeepBelow(bool) override;
    virtual int currentDesktop() const override;
    virtual Qt::WindowFlags initialWFlags() const override;
    virtual void grabXServer(bool grab) override;

    virtual bool compositingActive() const override;
    virtual QRect transparentRect() const override;

    virtual void update(const QRegion &region) override;
    virtual QPalette palette() const override;

    // Window tabbing
    virtual QString caption(int idx) const override;
    virtual void closeTab(long id) override;
    virtual void closeTabGroup() override;
    virtual long currentTabId() const override;
    virtual QIcon icon(int idx) const override;
    virtual void setCurrentTab(long id) override;
    virtual void showWindowMenu(const QPoint &, long id) override;
    virtual void tab_A_before_B(long A, long B) override;
    virtual void tab_A_behind_B(long A, long B) override;
    virtual int tabCount() const override;
    virtual long tabId(int idx) const override;
    virtual void untab(long id, const QRect& newGeom) override;

    virtual WindowOperation buttonToWindowOperation(Qt::MouseButtons button) override;

private:
    Client *clientForId(long id) const;
    static inline long tabIdOf(Client *c) {
        return reinterpret_cast<long>(c);
    }
    Client* c;
};

} // namespace

#endif

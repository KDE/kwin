/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_MOCK_TABBOX_HANDLER_H
#define KWIN_MOCK_TABBOX_HANDLER_H

#include "../tabboxhandler.h"
namespace KWin
{
class MockTabBoxHandler : public TabBox::TabBoxHandler
{
    Q_OBJECT
public:
    MockTabBoxHandler();
    virtual ~MockTabBoxHandler();
    virtual void activateAndClose() {
    }
    virtual QWeakPointer< TabBox::TabBoxClient > activeClient() const;
    virtual int activeScreen() const {
        return 0;
    }
    virtual QWeakPointer< TabBox::TabBoxClient > clientToAddToList(TabBox::TabBoxClient *client, int desktop) const;
    virtual int currentDesktop() const {
        return 1;
    }
    virtual QWeakPointer< TabBox::TabBoxClient > desktopClient() const {
        return QWeakPointer<TabBox::TabBoxClient>();
    }
    virtual QString desktopName(int desktop) const {
        Q_UNUSED(desktop)
        return "desktop 1";
    }
    virtual QString desktopName(TabBox::TabBoxClient *client) const {
        Q_UNUSED(client)
        return "desktop";
    }
    virtual void elevateClient(TabBox::TabBoxClient *c, WId tabbox, bool elevate) const {
        Q_UNUSED(c)
        Q_UNUSED(tabbox)
        Q_UNUSED(elevate)
    }
    virtual void hideOutline() {
    }
    virtual QWeakPointer< TabBox::TabBoxClient > nextClientFocusChain(TabBox::TabBoxClient *client) const;
    virtual int nextDesktopFocusChain(int desktop) const {
        Q_UNUSED(desktop)
        return 1;
    }
    virtual int numberOfDesktops() const {
        return 1;
    }
    virtual QVector< Window > outlineWindowIds() const {
        return QVector<Window>();
    }
    virtual void raiseClient(TabBox::TabBoxClient *c) const {
        Q_UNUSED(c)
    }
    virtual void restack(TabBox::TabBoxClient *c, TabBox::TabBoxClient *under) {
        Q_UNUSED(c)
        Q_UNUSED(under)
    }
    virtual void showOutline(const QRect &outline) {
        Q_UNUSED(outline)
    }
    virtual TabBox::TabBoxClientList stackingOrder() const {
        return TabBox::TabBoxClientList();
    }
    virtual void grabbedKeyEvent(QKeyEvent *event) const;

    // mock methods
    QWeakPointer<TabBox::TabBoxClient> createMockWindow(const QString &caption, WId id);
    void closeWindow(TabBox::TabBoxClient *client);
private:
    QList< QSharedPointer<TabBox::TabBoxClient> > m_windows;
};
} // namespace KWin
#endif

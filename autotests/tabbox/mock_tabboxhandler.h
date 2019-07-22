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

#include "../../tabbox/tabboxhandler.h"
namespace KWin
{
class MockTabBoxHandler : public TabBox::TabBoxHandler
{
    Q_OBJECT
public:
    MockTabBoxHandler(QObject *parent = nullptr);
    ~MockTabBoxHandler() override;
    void activateAndClose() override {
    }
    QWeakPointer< TabBox::TabBoxClient > activeClient() const override;
    void setActiveClient(const QWeakPointer<TabBox::TabBoxClient> &client);
    int activeScreen() const override {
        return 0;
    }
    QWeakPointer< TabBox::TabBoxClient > clientToAddToList(TabBox::TabBoxClient *client, int desktop) const override;
    int currentDesktop() const override {
        return 1;
    }
    QWeakPointer< TabBox::TabBoxClient > desktopClient() const override {
        return QWeakPointer<TabBox::TabBoxClient>();
    }
    QString desktopName(int desktop) const override {
        Q_UNUSED(desktop)
        return "desktop 1";
    }
    QString desktopName(TabBox::TabBoxClient *client) const override {
        Q_UNUSED(client)
        return "desktop";
    }
    void elevateClient(TabBox::TabBoxClient *c, QWindow *tabbox, bool elevate) const override {
        Q_UNUSED(c)
        Q_UNUSED(tabbox)
        Q_UNUSED(elevate)
    }
    void shadeClient(TabBox::TabBoxClient *c, bool b) const override {
        Q_UNUSED(c)
        Q_UNUSED(b)
    }
    virtual void hideOutline() {
    }
    QWeakPointer< TabBox::TabBoxClient > nextClientFocusChain(TabBox::TabBoxClient *client) const override;
    QWeakPointer<TabBox::TabBoxClient> firstClientFocusChain() const override;
    bool isInFocusChain (TabBox::TabBoxClient* client) const override;
    int nextDesktopFocusChain(int desktop) const override {
        Q_UNUSED(desktop)
        return 1;
    }
    int numberOfDesktops() const override {
        return 1;
    }
    virtual QVector< xcb_window_t > outlineWindowIds() const {
        return QVector<xcb_window_t>();
    }
    bool isKWinCompositing() const override {
        return false;
    }
    void raiseClient(TabBox::TabBoxClient *c) const override {
        Q_UNUSED(c)
    }
    void restack(TabBox::TabBoxClient *c, TabBox::TabBoxClient *under) override {
        Q_UNUSED(c)
        Q_UNUSED(under)
    }
    virtual void showOutline(const QRect &outline) {
        Q_UNUSED(outline)
    }
    TabBox::TabBoxClientList stackingOrder() const override {
        return TabBox::TabBoxClientList();
    }
    void grabbedKeyEvent(QKeyEvent *event) const override;

    void highlightWindows(TabBox::TabBoxClient *window = nullptr, QWindow *controller = nullptr) override {
        Q_UNUSED(window)
        Q_UNUSED(controller)
    }

    bool noModifierGrab() const override {
        return false;
    }

    // mock methods
    QWeakPointer<TabBox::TabBoxClient> createMockWindow(const QString &caption, WId id);
    void closeWindow(TabBox::TabBoxClient *client);
private:
    QList< QSharedPointer<TabBox::TabBoxClient> > m_windows;
    QWeakPointer<TabBox::TabBoxClient> m_activeClient;
};
} // namespace KWin
#endif

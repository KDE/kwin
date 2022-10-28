/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MOCK_TABBOX_HANDLER_H
#define KWIN_MOCK_TABBOX_HANDLER_H

#include "tabbox/tabboxhandler.h"
namespace KWin
{
class MockTabBoxHandler : public TabBox::TabBoxHandler
{
    Q_OBJECT
public:
    MockTabBoxHandler(QObject *parent = nullptr);
    ~MockTabBoxHandler() override;
    void activateAndClose() override
    {
    }
    QWeakPointer<TabBox::TabBoxClient> activeClient() const override;
    void setActiveClient(const QWeakPointer<TabBox::TabBoxClient> &client);
    int activeScreen() const override
    {
        return 0;
    }
    QWeakPointer<TabBox::TabBoxClient> clientToAddToList(TabBox::TabBoxClient *client, int desktop) const override;
    int currentDesktop() const override
    {
        return 1;
    }
    QWeakPointer<TabBox::TabBoxClient> desktopClient() const override
    {
        return QWeakPointer<TabBox::TabBoxClient>();
    }
    QString desktopName(int desktop) const override
    {
        return "desktop 1";
    }
    QString desktopName(TabBox::TabBoxClient *client) const override
    {
        return "desktop";
    }
    void elevateClient(TabBox::TabBoxClient *c, QWindow *tabbox, bool elevate) const override
    {
    }
    void shadeClient(TabBox::TabBoxClient *c, bool b) const override
    {
    }
    virtual void hideOutline()
    {
    }
    QWeakPointer<TabBox::TabBoxClient> nextClientFocusChain(TabBox::TabBoxClient *client) const override;
    QWeakPointer<TabBox::TabBoxClient> firstClientFocusChain() const override;
    bool isInFocusChain(TabBox::TabBoxClient *client) const override;
    int nextDesktopFocusChain(int desktop) const override
    {
        return 1;
    }
    int numberOfDesktops() const override
    {
        return 1;
    }
    bool isKWinCompositing() const override
    {
        return false;
    }
    void raiseClient(TabBox::TabBoxClient *c) const override
    {
    }
    void restack(TabBox::TabBoxClient *c, TabBox::TabBoxClient *under) override
    {
    }
    virtual void showOutline(const QRect &outline)
    {
    }
    TabBox::TabBoxClientList stackingOrder() const override
    {
        return TabBox::TabBoxClientList();
    }
    void grabbedKeyEvent(QKeyEvent *event) const override;

    void highlightWindows(TabBox::TabBoxClient *window = nullptr, QWindow *controller = nullptr) override
    {
    }

    bool noModifierGrab() const override
    {
        return false;
    }

    // mock methods
    QWeakPointer<TabBox::TabBoxClient> createMockWindow(const QString &caption);
    void closeWindow(TabBox::TabBoxClient *client);

private:
    QList<QSharedPointer<TabBox::TabBoxClient>> m_windows;
    QWeakPointer<TabBox::TabBoxClient> m_activeClient;
};
} // namespace KWin
#endif

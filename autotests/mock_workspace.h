/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MOCK_WORKSPACE_H
#define KWIN_MOCK_WORKSPACE_H

#include <QObject>
#include <kwinglobals.h>

namespace KWin
{

class AbstractClient;
class X11Client;
class X11EventFilter;

class MockWorkspace;
typedef MockWorkspace Workspace;

class MockWorkspace : public QObject
{
    Q_OBJECT
public:
    explicit MockWorkspace(QObject *parent = nullptr);
    ~MockWorkspace() override;
    AbstractClient *activeClient() const;
    AbstractClient *moveResizeClient() const;
    void setShowingDesktop(bool showing);
    bool showingDesktop() const;
    QRect clientArea(clientAreaOption, int screen, int desktop) const;

    void setActiveClient(AbstractClient *c);
    void setMoveResizeClient(AbstractClient *c);

    void registerEventFilter(X11EventFilter *filter);
    void unregisterEventFilter(X11EventFilter *filter);

    bool compositing() const {
        return false;
    }

    static Workspace *self();

Q_SIGNALS:
    void clientRemoved(KWin::X11Client *);

private:
    AbstractClient *m_activeClient;
    AbstractClient *m_moveResizeClient;
    bool m_showingDesktop;
    static Workspace *s_self;
};

inline
Workspace *MockWorkspace::self()
{
    return s_self;
}

inline Workspace *workspace()
{
    return Workspace::self();
}

}

#endif

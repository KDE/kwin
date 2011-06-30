/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_TABBOX_H
#define KWIN_TABBOX_H

#include <QTimer>
#include <QModelIndex>
#include "utils.h"
#include "tabbox/tabboxhandler.h"

class QKeyEvent;

namespace KWin
{

class Workspace;
class Client;
namespace TabBox
{
class TabBoxConfig;
class TabBox;
class TabBoxHandlerImpl : public TabBoxHandler
{
public:
    TabBoxHandlerImpl(TabBox* tabBox);
    virtual ~TabBoxHandlerImpl();

    virtual int activeScreen() const;
    virtual TabBoxClient* activeClient() const;
    virtual int currentDesktop() const;
    virtual QString desktopName(TabBoxClient* client) const;
    virtual QString desktopName(int desktop) const;
    virtual TabBoxClient* nextClientFocusChain(TabBoxClient* client) const;
    virtual int nextDesktopFocusChain(int desktop) const;
    virtual int numberOfDesktops() const;
    virtual TabBoxClientList stackingOrder() const;
    virtual void raiseClient(TabBoxClient *client) const;
    virtual void restack(TabBoxClient *c, TabBoxClient *under);
    virtual TabBoxClient* clientToAddToList(TabBoxClient* client, int desktop, bool allDesktops) const;
    virtual TabBoxClient* desktopClient() const;
    virtual void hideOutline();
    virtual void showOutline(const QRect &outline);
    virtual QVector< Window > outlineWindowIds() const;

private:
    TabBox* m_tabBox;
};

class TabBoxClientImpl : public TabBoxClient
{
public:
    TabBoxClientImpl();
    virtual ~TabBoxClientImpl();

    virtual QString caption() const;
    virtual QPixmap icon(const QSize& size = QSize(32, 32)) const;
    virtual WId window() const;
    virtual bool isMinimized() const;
    virtual int x() const;
    virtual int y() const;
    virtual int width() const;
    virtual int height() const;

    Client* client() const {
        return m_client;
    }
    void setClient(Client* client) {
        m_client = client;
    }

private:
    Client* m_client;
};

class TabBox : public QObject
{
    Q_OBJECT
public:
    TabBox(QObject *parent = NULL);
    ~TabBox();

    Client* currentClient();
    ClientList currentClientList();
    int currentDesktop();
    QList< int > currentDesktopList();

    void setCurrentClient(Client* newClient);
    void setCurrentDesktop(int newDesktop);

    void setMode(TabBoxMode mode);
    TabBoxMode mode() const {
        return m_tabBoxMode;
    }

    void reset(bool partial_reset = false);
    void nextPrev(bool next = true);

    void delayedShow();
    void hide(bool abort = false);

    /*!
    Increase the reference count, preventing the default tabbox from showing.

    \sa unreference(), isDisplayed()
    */
    void reference() {
        ++m_displayRefcount;
    }
    /*!
    Decrease the reference count.  Only when the reference count is 0 will
    the default tab box be shown.
    */
    void unreference() {
        --m_displayRefcount;
    }
    /*!
    Returns whether the tab box is being displayed, either natively or by an
    effect.

    \sa reference(), unreference()
    */
    bool isDisplayed() const {
        return m_displayRefcount > 0;
    };

    void handleMouseEvent(XEvent*);
    void grabbedKeyEvent(QKeyEvent* event);

    void reconfigure();

    bool isGrabbed() const {
        return m_tabGrab || m_desktopGrab;
    };

    void initShortcuts(KActionCollection* keys);

    Client* nextClientFocusChain(Client*) const;
    Client* previousClientFocusChain(Client*) const;
    Client* nextClientStatic(Client*) const;
    Client* previousClientStatic(Client*) const;
    int nextDesktopFocusChain(int iDesktop) const;
    int previousDesktopFocusChain(int iDesktop) const;
    int nextDesktopStatic(int iDesktop) const;
    int previousDesktopStatic(int iDesktop) const;
    void close(bool abort = false);
    void keyPress(int key);
    void keyRelease(const XKeyEvent& ev);

public slots:
    void show();
    void slotWalkThroughDesktops();
    void slotWalkBackThroughDesktops();
    void slotWalkThroughDesktopList();
    void slotWalkBackThroughDesktopList();
    void slotWalkThroughWindows();
    void slotWalkBackThroughWindows();
    void slotWalkThroughWindowsAlternative();
    void slotWalkBackThroughWindowsAlternative();

    void slotWalkThroughDesktopsKeyChanged(const QKeySequence& seq);
    void slotWalkBackThroughDesktopsKeyChanged(const QKeySequence& seq);
    void slotWalkThroughDesktopListKeyChanged(const QKeySequence& seq);
    void slotWalkBackThroughDesktopListKeyChanged(const QKeySequence& seq);
    void slotWalkThroughWindowsKeyChanged(const QKeySequence& seq);
    void slotWalkBackThroughWindowsKeyChanged(const QKeySequence& seq);
    void slotMoveToTabLeftKeyChanged(const QKeySequence& seq);
    void slotMoveToTabRightKeyChanged(const QKeySequence& seq);
    void slotWalkThroughWindowsAlternativeKeyChanged(const QKeySequence& seq);
    void slotWalkBackThroughWindowsAlternativeKeyChanged(const QKeySequence& seq);

signals:
    void tabBoxAdded(int);
    void tabBoxClosed();
    void tabBoxUpdated();
    void tabBoxKeyEvent(QKeyEvent*);

private:
    void setCurrentIndex(QModelIndex index, bool notifyEffects = true);
    void loadConfig(const KConfigGroup& config, TabBoxConfig& tabBoxConfig);

    bool startKDEWalkThroughWindows(TabBoxMode mode);   // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    bool startWalkThroughDesktops(TabBoxMode mode);   // TabBoxDesktopMode | TabBoxDesktopListMode
    bool startWalkThroughDesktops();
    bool startWalkThroughDesktopList();
    void navigatingThroughWindows(bool forward, const KShortcut& shortcut, TabBoxMode mode);   // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    void KDEWalkThroughWindows(bool forward);
    void CDEWalkThroughWindows(bool forward);
    void walkThroughDesktops(bool forward);
    void KDEOneStepThroughWindows(bool forward, TabBoxMode mode);   // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    void oneStepThroughDesktops(bool forward, TabBoxMode mode);   // TabBoxDesktopMode | TabBoxDesktopListMode
    void oneStepThroughDesktops(bool forward);
    void oneStepThroughDesktopList(bool forward);
    bool establishTabBoxGrab();
    void removeTabBoxGrab();
    void modalActionsSwitch(bool enabled);

private:
    TabBoxMode m_tabBoxMode;
    QModelIndex m_index;
    TabBoxHandlerImpl* m_tabBox;
    bool m_delayShow;
    int m_delayShowTime;

    QTimer m_delayedShowTimer;
    int m_displayRefcount;

    TabBoxConfig m_defaultConfig;
    TabBoxConfig m_alternativeConfig;
    TabBoxConfig m_desktopConfig;
    TabBoxConfig m_desktopListConfig;
    // false if an effect has referenced the tabbox
    // true if tabbox is active (independent on showTabbox setting)
    bool m_isShown;
    bool m_desktopGrab;
    bool m_tabGrab;
    KShortcut m_cutWalkThroughDesktops, m_cutWalkThroughDesktopsReverse;
    KShortcut m_cutWalkThroughDesktopList, m_cutWalkThroughDesktopListReverse;
    KShortcut m_cutWalkThroughWindows, m_cutWalkThroughWindowsReverse;
    KShortcut m_cutWalkThroughGroupWindows, m_cutWalkThroughGroupWindowsReverse;
    KShortcut m_cutWalkThroughWindowsAlternative, m_cutWalkThroughWindowsAlternativeReverse;
    bool m_forcedGlobalMouseGrab;
};
} // namespace TabBox
} // namespace
#endif

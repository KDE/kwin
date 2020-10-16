/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_TABBOX_H
#define KWIN_TABBOX_H

#include <QKeySequence>
#include <QTimer>
#include <QModelIndex>
#include "utils.h"
#include "tabbox/tabboxhandler.h"

class KConfigGroup;
class QAction;
class QMouseEvent;
class QKeyEvent;
class QWheelEvent;

namespace KWin
{

class Workspace;
class AbstractClient;
class X11EventFilter;
namespace TabBox
{
class DesktopChainManager;
class TabBoxConfig;
class TabBox;
class TabBoxHandlerImpl : public TabBoxHandler
{
public:
    explicit TabBoxHandlerImpl(TabBox* tabBox);
    ~TabBoxHandlerImpl() override;

    int activeScreen() const override;
    QWeakPointer< TabBoxClient > activeClient() const override;
    int currentDesktop() const override;
    QString desktopName(TabBoxClient* client) const override;
    QString desktopName(int desktop) const override;
    bool isKWinCompositing() const override;
    QWeakPointer< TabBoxClient > nextClientFocusChain(TabBoxClient* client) const override;
    QWeakPointer< TabBoxClient > firstClientFocusChain() const override;
    bool isInFocusChain (TabBoxClient* client) const override;
    int nextDesktopFocusChain(int desktop) const override;
    int numberOfDesktops() const override;
    TabBoxClientList stackingOrder() const override;
    void elevateClient(TabBoxClient* c, QWindow *tabbox, bool elevate) const override;
    void raiseClient(TabBoxClient *client) const override;
    void restack(TabBoxClient *c, TabBoxClient *under) override;
    void shadeClient(TabBoxClient *c, bool b) const override;
    QWeakPointer< TabBoxClient > clientToAddToList(KWin::TabBox::TabBoxClient* client, int desktop) const override;
    QWeakPointer< TabBoxClient > desktopClient() const override;
    void activateAndClose() override;
    void highlightWindows(TabBoxClient *window = nullptr, QWindow *controller = nullptr) override;
    bool noModifierGrab() const override;

private:
    bool checkDesktop(TabBoxClient* client, int desktop) const;
    bool checkActivity(TabBoxClient* client) const;
    bool checkApplications(TabBoxClient* client) const;
    bool checkMinimized(TabBoxClient* client) const;
    bool checkMultiScreen(TabBoxClient* client) const;

    TabBox* m_tabBox;
    DesktopChainManager* m_desktopFocusChain;
};

class TabBoxClientImpl : public TabBoxClient
{
public:
    explicit TabBoxClientImpl(AbstractClient *client);
    ~TabBoxClientImpl() override;

    QString caption() const override;
    QIcon icon() const override;
    bool isMinimized() const override;
    int x() const override;
    int y() const override;
    int width() const override;
    int height() const override;
    bool isCloseable() const override;
    void close() override;
    bool isFirstInTabBox() const override;
    QUuid internalId() const override;

    AbstractClient* client() const {
        return m_client;
    }

private:
    AbstractClient* m_client;
};

class KWIN_EXPORT TabBox : public QObject
{
    Q_OBJECT
public:
    ~TabBox() override;

    /**
     * Returns the currently displayed client ( only works in TabBoxWindowsMode ).
     * Returns 0 if no client is displayed.
     */
    AbstractClient *currentClient();

    /**
     * Returns the list of clients potentially displayed ( only works in
     * TabBoxWindowsMode ).
     * Returns an empty list if no clients are available.
     */
    QList<AbstractClient*> currentClientList();

    /**
     * Returns the currently displayed virtual desktop ( only works in
     * TabBoxDesktopListMode )
     * Returns -1 if no desktop is displayed.
     */
    int currentDesktop();

    /**
     * Returns the list of desktops potentially displayed ( only works in
     * TabBoxDesktopListMode )
     * Returns an empty list if no are available.
     */
    QList< int > currentDesktopList();

    /**
     * Change the currently selected client, and notify the effects.
     *
     * @see setCurrentDesktop
     */
    void setCurrentClient(AbstractClient *newClient);

    /**
     * Change the currently selected desktop, and notify the effects.
     *
     * @see setCurrentClient
     */
    void setCurrentDesktop(int newDesktop);

    /**
     * Sets the current mode to \a mode, either TabBoxDesktopListMode or TabBoxWindowsMode
     *
     * @see mode
     */
    void setMode(TabBoxMode mode);
    TabBoxMode mode() const {
        return m_tabBoxMode;
    }

    /**
     * Resets the tab box to display the active client in TabBoxWindowsMode, or the
     * current desktop in TabBoxDesktopListMode
     */
    void reset(bool partial_reset = false);

    /**
     * Shows the next or previous item, depending on \a next
     */
    void nextPrev(bool next = true);

    /**
     * Shows the tab box after some delay.
     *
     * If the 'ShowDelay' setting is false, show() is simply called.
     *
     * Otherwise, we start a timer for the delay given in the settings and only
     * do a show() when it times out.
     *
     * This means that you can alt-tab between windows and you don't see the
     * tab box immediately. Not only does this make alt-tabbing faster, it gives
     * less 'flicker' to the eyes. You don't need to see the tab box if you're
     * just quickly switching between 2 or 3 windows. It seems to work quite
     * nicely.
     */
    void delayedShow();

    /**
     * Notify effects that the tab box is being hidden.
     */
    void hide(bool abort = false);

    /**
     * Increases the reference count, preventing the default tabbox from showing.
     *
     * @see unreference
     * @see isDisplayed
     */
    void reference() {
        ++m_displayRefcount;
    }

    /**
     * Decreases the reference count. Only when the reference count is 0 will
     * the default tab box be shown.
     */
    void unreference() {
        --m_displayRefcount;
    }

    /**
     * Returns whether the tab box is being displayed, either natively or by an
     * effect.
     *
     * @see reference
     * @see unreference
     */
    bool isDisplayed() const {
        return m_displayRefcount > 0;
    }

    /**
     * @returns @c true if TabBox is shown, @c false if replaced by Effect
     */
    bool isShown() const {
        return m_isShown;
    }

    bool handleMouseEvent(QMouseEvent *event);
    bool handleWheelEvent(QWheelEvent *event);
    void grabbedKeyEvent(QKeyEvent* event);

    bool isGrabbed() const {
        return m_tabGrab || m_desktopGrab;
    }

    void initShortcuts();

    AbstractClient* nextClientStatic(AbstractClient*) const;
    AbstractClient* previousClientStatic(AbstractClient*) const;
    int nextDesktopStatic(int iDesktop) const;
    int previousDesktopStatic(int iDesktop) const;
    void keyPress(int key);
    void modifiersReleased();

    bool forcedGlobalMouseGrab() const {
        return m_forcedGlobalMouseGrab;
    }

    bool noModifierGrab() const {
        return m_noModifierGrab;
    }
    void setCurrentIndex(QModelIndex index, bool notifyEffects = true);

    static TabBox *self();
    static TabBox *create(QObject *parent);

public Q_SLOTS:
    /**
     * Notify effects that the tab box is being shown, and only display the
     * default tab box QFrame if no effect has referenced the tab box.
     */
    void show();
    void close(bool abort = false);
    void accept(bool closeTabBox = true);
    void slotWalkThroughDesktops();
    void slotWalkBackThroughDesktops();
    void slotWalkThroughDesktopList();
    void slotWalkBackThroughDesktopList();
    void slotWalkThroughWindows();
    void slotWalkBackThroughWindows();
    void slotWalkThroughWindowsAlternative();
    void slotWalkBackThroughWindowsAlternative();
    void slotWalkThroughCurrentAppWindows();
    void slotWalkBackThroughCurrentAppWindows();
    void slotWalkThroughCurrentAppWindowsAlternative();
    void slotWalkBackThroughCurrentAppWindowsAlternative();

    void handlerReady();

    bool toggle(ElectricBorder eb);

Q_SIGNALS:
    void tabBoxAdded(int);
    void tabBoxClosed();
    void tabBoxUpdated();
    void tabBoxKeyEvent(QKeyEvent*);

private:
    explicit TabBox(QObject *parent);
    void loadConfig(const KConfigGroup& config, TabBoxConfig& tabBoxConfig);

    bool startKDEWalkThroughWindows(TabBoxMode mode);   // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    bool startWalkThroughDesktops(TabBoxMode mode);   // TabBoxDesktopMode | TabBoxDesktopListMode
    bool startWalkThroughDesktops();
    bool startWalkThroughDesktopList();
    void navigatingThroughWindows(bool forward, const QKeySequence &shortcut, TabBoxMode mode);   // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    void KDEWalkThroughWindows(bool forward);
    void CDEWalkThroughWindows(bool forward);
    void walkThroughDesktops(bool forward);
    void KDEOneStepThroughWindows(bool forward, TabBoxMode mode);   // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    void oneStepThroughDesktops(bool forward, TabBoxMode mode);   // TabBoxDesktopMode | TabBoxDesktopListMode
    void oneStepThroughDesktops(bool forward);
    void oneStepThroughDesktopList(bool forward);
    bool establishTabBoxGrab();
    void removeTabBoxGrab();
    template <typename Slot>
    void key(const char *actionName, Slot slot, const QKeySequence &shortcut = QKeySequence());

    void shadeActivate(AbstractClient *c);

    bool toggleMode(TabBoxMode mode);

private Q_SLOTS:
    void reconfigure();
    void globalShortcutChanged(QAction *action, const QKeySequence &seq);

private:
    TabBoxMode m_tabBoxMode;
    TabBoxHandlerImpl* m_tabBox;
    bool m_delayShow;
    int m_delayShowTime;

    QTimer m_delayedShowTimer;
    int m_displayRefcount;

    TabBoxConfig m_defaultConfig;
    TabBoxConfig m_alternativeConfig;
    TabBoxConfig m_defaultCurrentApplicationConfig;
    TabBoxConfig m_alternativeCurrentApplicationConfig;
    TabBoxConfig m_desktopConfig;
    TabBoxConfig m_desktopListConfig;
    // false if an effect has referenced the tabbox
    // true if tabbox is active (independent on showTabbox setting)
    bool m_isShown;
    bool m_desktopGrab;
    bool m_tabGrab;
    // true if tabbox is in modal mode which does not require holding a modifier
    bool m_noModifierGrab;
    QKeySequence m_cutWalkThroughDesktops, m_cutWalkThroughDesktopsReverse;
    QKeySequence m_cutWalkThroughDesktopList, m_cutWalkThroughDesktopListReverse;
    QKeySequence m_cutWalkThroughWindows, m_cutWalkThroughWindowsReverse;
    QKeySequence m_cutWalkThroughWindowsAlternative, m_cutWalkThroughWindowsAlternativeReverse;
    QKeySequence m_cutWalkThroughCurrentAppWindows, m_cutWalkThroughCurrentAppWindowsReverse;
    QKeySequence m_cutWalkThroughCurrentAppWindowsAlternative, m_cutWalkThroughCurrentAppWindowsAlternativeReverse;
    bool m_forcedGlobalMouseGrab;
    bool m_ready; // indicates whether the config is completely loaded
    QList<ElectricBorder> m_borderActivate, m_borderAlternativeActivate;
    QHash<ElectricBorder, QAction *> m_touchActivate;
    QHash<ElectricBorder, QAction *> m_touchAlternativeActivate;
    QScopedPointer<X11EventFilter> m_x11EventFilter;

    static TabBox *s_self;
};

inline
TabBox *TabBox::self()
{
    return s_self;
}

} // namespace TabBox
} // namespace
#endif

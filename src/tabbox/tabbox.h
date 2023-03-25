/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tabbox/tabboxhandler.h"
#include "utils/common.h"
#include <QKeySequence>
#include <QModelIndex>
#include <QTimer>

class KConfigGroup;
class KLazyLocalizedString;
class QAction;
class QMouseEvent;
class QKeyEvent;
class QWheelEvent;

namespace KWin
{

class Workspace;
class Window;
class X11EventFilter;
namespace TabBox
{
class TabBoxConfig;
class TabBox;
class TabBoxHandlerImpl : public TabBoxHandler
{
public:
    explicit TabBoxHandlerImpl(TabBox *tabBox);
    ~TabBoxHandlerImpl() override;

    int activeScreen() const override;
    Window *activeClient() const override;
    int currentDesktop() const override;
    QString desktopName(Window *client) const override;
    bool isKWinCompositing() const override;
    Window *nextClientFocusChain(Window *client) const override;
    Window *firstClientFocusChain() const override;
    bool isInFocusChain(Window *client) const override;
    QList<Window *> stackingOrder() const override;
    void elevateClient(Window *c, QWindow *tabbox, bool elevate) const override;
    void raiseClient(Window *client) const override;
    void restack(Window *c, Window *under) override;
    void shadeClient(Window *c, bool b) const override;
    Window *clientToAddToList(Window *client, int desktop) const override;
    Window *desktopClient() const override;
    void activateAndClose() override;
    void highlightWindows(Window *window = nullptr, QWindow *controller = nullptr) override;
    bool noModifierGrab() const override;

private:
    bool checkDesktop(Window *client, int desktop) const;
    bool checkActivity(Window *client) const;
    bool checkApplications(Window *client) const;
    bool checkMinimized(Window *client) const;
    bool checkMultiScreen(Window *client) const;

    TabBox *m_tabBox;
};

class KWIN_EXPORT TabBox : public QObject
{
    Q_OBJECT
public:
    explicit TabBox();
    ~TabBox();

    /**
     * Returns the currently displayed client ( only works in TabBoxWindowsMode ).
     * Returns 0 if no client is displayed.
     */
    Window *currentClient();

    /**
     * Returns the list of clients potentially displayed ( only works in
     * TabBoxWindowsMode ).
     * Returns an empty list if no clients are available.
     */
    QList<Window *> currentClientList();

    /**
     * Change the currently selected client, and notify the effects.
     */
    void setCurrentClient(Window *newClient);

    void setMode(TabBoxMode mode);
    TabBoxMode mode() const
    {
        return m_tabBoxMode;
    }

    /**
     * Resets the tab box to display the active client in TabBoxWindowsMode
     */
    void reset(bool partial_reset = false);

    /**
     * Shows the next or previous item, depending on \a next
     */
    void nextPrev(bool next = true);

    /**
     * Shows the tab box after some delay.
     *
     * If the 'DelayTime' setting is 0, show() is simply called.
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
    void reference()
    {
        ++m_displayRefcount;
    }

    /**
     * Decreases the reference count. Only when the reference count is 0 will
     * the default tab box be shown.
     */
    void unreference()
    {
        --m_displayRefcount;
    }

    /**
     * Returns whether the tab box is being displayed, either natively or by an
     * effect.
     *
     * @see reference
     * @see unreference
     */
    bool isDisplayed() const
    {
        return m_displayRefcount > 0;
    }

    /**
     * @returns @c true if TabBox is shown, @c false if replaced by Effect
     */
    bool isShown() const
    {
        return m_isShown;
    }

    bool handleMouseEvent(QMouseEvent *event);
    bool handleWheelEvent(QWheelEvent *event);
    void grabbedKeyEvent(QKeyEvent *event);

    bool isGrabbed() const
    {
        return m_tabGrab;
    }

    void initShortcuts();

    Window *nextClientStatic(Window *) const;
    Window *previousClientStatic(Window *) const;
    void keyPress(int key);
    void modifiersReleased();

    bool forcedGlobalMouseGrab() const
    {
        return m_forcedGlobalMouseGrab;
    }

    bool noModifierGrab() const
    {
        return m_noModifierGrab;
    }
    void setCurrentIndex(QModelIndex index, bool notifyEffects = true);

public Q_SLOTS:
    /**
     * Notify effects that the tab box is being shown, and only display the
     * default tab box QFrame if no effect has referenced the tab box.
     */
    void show();
    void close(bool abort = false);
    void accept(bool closeTabBox = true);
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
    void tabBoxKeyEvent(QKeyEvent *);

private:
    explicit TabBox(QObject *parent);
    void loadConfig(const KConfigGroup &config, TabBoxConfig &tabBoxConfig);

    bool startKDEWalkThroughWindows(TabBoxMode mode); // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    void navigatingThroughWindows(bool forward, const QKeySequence &shortcut, TabBoxMode mode); // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    void KDEWalkThroughWindows(bool forward);
    void CDEWalkThroughWindows(bool forward);
    void KDEOneStepThroughWindows(bool forward, TabBoxMode mode); // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
    bool establishTabBoxGrab();
    void removeTabBoxGrab();
    template<typename Slot>
    void key(const KLazyLocalizedString &actionName, Slot slot, const QKeySequence &shortcut = QKeySequence());

    void shadeActivate(Window *c);

    bool toggleMode(TabBoxMode mode);

private Q_SLOTS:
    void reconfigure();
    void globalShortcutChanged(QAction *action, const QKeySequence &seq);

private:
    TabBoxMode m_tabBoxMode;
    TabBoxHandlerImpl *m_tabBox;
    int m_delayShowTime;

    QTimer m_delayedShowTimer;
    int m_displayRefcount;

    TabBoxConfig m_defaultConfig;
    TabBoxConfig m_alternativeConfig;
    TabBoxConfig m_defaultCurrentApplicationConfig;
    TabBoxConfig m_alternativeCurrentApplicationConfig;
    // false if an effect has referenced the tabbox
    // true if tabbox is active (independent on showTabbox setting)
    bool m_isShown;
    bool m_tabGrab;
    // true if tabbox is in modal mode which does not require holding a modifier
    bool m_noModifierGrab;
    QKeySequence m_cutWalkThroughWindows, m_cutWalkThroughWindowsReverse;
    QKeySequence m_cutWalkThroughWindowsAlternative, m_cutWalkThroughWindowsAlternativeReverse;
    QKeySequence m_cutWalkThroughCurrentAppWindows, m_cutWalkThroughCurrentAppWindowsReverse;
    QKeySequence m_cutWalkThroughCurrentAppWindowsAlternative, m_cutWalkThroughCurrentAppWindowsAlternativeReverse;
    bool m_forcedGlobalMouseGrab;
    bool m_ready; // indicates whether the config is completely loaded
    QList<ElectricBorder> m_borderActivate, m_borderAlternativeActivate;
    QHash<ElectricBorder, QAction *> m_touchActivate;
    QHash<ElectricBorder, QAction *> m_touchAlternativeActivate;
    std::unique_ptr<X11EventFilter> m_x11EventFilter;
};

} // namespace TabBox
} // namespace

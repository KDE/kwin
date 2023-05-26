/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 This file contains things relevant to window activation and focus
 stealing prevention.

*/

#include "cursor.h"
#include "focuschain.h"
#include "netinfo.h"
#include "workspace.h"
#include "x11window.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "virtualdesktops.h"

#include <KLocalizedString>
#include <kstartupinfo.h>
#include <kstringhandler.h>

#include "atoms.h"
#include "group.h"
#include "rules.h"
#include "useractions.h"
#include <QDebug>

namespace KWin
{

/*
 Prevention of focus stealing:

 KWin tries to prevent unwanted changes of focus, that would result
 from mapping a new window. Also, some nasty applications may try
 to force focus change even in cases when ICCCM 4.2.7 doesn't allow it
 (e.g. they may try to activate their main window because the user
 definitely "needs" to see something happened - misusing
 of QWidget::setActiveWindow() may be such case).

 There are 4 ways how a window may become active:
 - the user changes the active window (e.g. focus follows mouse, clicking
   on some window's titlebar) - the change of focus will
   be done by KWin, so there's nothing to solve in this case
 - the change of active window will be requested using the _NET_ACTIVE_WINDOW
   message (handled in RootInfo::changeActiveWindow()) - such requests
   will be obeyed, because this request is meant mainly for e.g. taskbar
   asking the WM to change the active window as a result of some user action.
   Normal applications should use this request only rarely in special cases.
   See also below the discussion of _NET_ACTIVE_WINDOW_TRANSFER.
 - the change of active window will be done by performing XSetInputFocus()
   on a window that's not currently active. ICCCM 4.2.7 describes when
   the application may perform change of input focus. In order to handle
   misbehaving applications, KWin will try to detect focus changes to
   windows that don't belong to currently active application, and restore
   focus back to the currently active window, instead of activating the window
   that got focus (unfortunately there's no way to FocusChangeRedirect similar
   to e.g. SubstructureRedirect, so there will be short time when the focus
   will be changed). The check itself that's done is
   Workspace::allowWindowActivation() (see below).
 - a new window will be mapped - this is the most complicated case. If
   the new window belongs to the currently active application, it may be safely
   mapped on top and activated. The same if there's no active window,
   or the active window is the desktop. These checks are done by
   Workspace::allowWindowActivation().
    Following checks need to compare times. One time is the timestamp
   of last user action in the currently active window, the other time is
   the timestamp of the action that originally caused mapping of the new window
   (e.g. when the application was started). If the first time is newer than
   the second one, the window will not be activated, as that indicates
   futher user actions took place after the action leading to this new
   mapped window. This check is done by Workspace::allowWindowActivation().
    There are several ways how to get the timestamp of action that caused
   the new mapped window (done in X11Window::readUserTimeMapTimestamp()) :
     - the window may have the _NET_WM_USER_TIME property. This way
       the application may either explicitly request that the window is not
       activated (by using 0 timestamp), or the property contains the time
       of last user action in the application.
     - KWin itself tries to detect time of last user action in every window,
       by watching KeyPress and ButtonPress events on windows. This way some
       events may be missed (if they don't propagate to the toplevel window),
       but it's good as a fallback for applications that don't provide
       _NET_WM_USER_TIME, and missing some events may at most lead
       to unwanted focus stealing.
     - the timestamp may come from application startup notification.
       Application startup notification, if it exists for the new mapped window,
       should include time of the user action that caused it.
     - if there's no timestamp available, it's checked whether the new window
       belongs to some already running application - if yes, the timestamp
       will be 0 (i.e. refuse activation)
     - if the window is from session restored window, the timestamp will
       be 0 too, unless this application was the active one at the time
       when the session was saved, in which case the window will be
       activated if there wasn't any user interaction since the time
       KWin was started.
     - as the last resort, the _KDE_NET_USER_CREATION_TIME timestamp
       is used. For every toplevel window that is created (see CreateNotify
       handling), this property is set to the at that time current time.
       Since at this time it's known that the new window doesn't belong
       to any existing application (better said, the application doesn't
       have any other window mapped), it is either the very first window
       of the application, or it is the only window of the application
       that was hidden before. The latter case is handled by removing
       the property from windows before withdrawing them, making
       the timestamp empty for next mapping of the window. In the sooner
       case, the timestamp will be used. This helps in case when
       an application is launched without application startup notification,
       it creates its mainwindow, and starts its initialization (that
       may possibly take long time). The timestamp used will be older
       than any user action done after launching this application.
     - if no timestamp is found at all, the window is activated.
    The check whether two windows belong to the same application (same
   process) is done in X11Window::belongToSameApplication(). Not 100% reliable,
   but hopefully 99,99% reliable.

 As a somewhat special case, window activation is always enabled when
 session saving is in progress. When session saving, the session
 manager allows only one application to interact with the user.
 Not allowing window activation in such case would result in e.g. dialogs
 not becoming active, so focus stealing prevention would cause here
 more harm than good.

 Windows that attempted to become active but KWin prevented this will
 be marked as demanding user attention. They'll get
 the _NET_WM_STATE_DEMANDS_ATTENTION state, and the taskbar should mark
 them specially (blink, etc.). The state will be reset when the window
 eventually really becomes active.

 There are two more ways how a window can become obtrusive, window stealing
 focus: By showing above the active window, by either raising itself,
 or by moving itself on the active desktop.
     - KWin will refuse raising non-active window above the active one,
         unless they belong to the same application. Applications shouldn't
         raise their windows anyway (unless the app wants to raise one
         of its windows above another of its windows).
     - KWin activates windows moved to the current desktop (as that seems
         logical from the user's point of view, after sending the window
         there directly from KWin, or e.g. using pager). This means
         applications shouldn't send their windows to another desktop
         (SELI TODO - but what if they do?)

 Special cases I can think of:
    - konqueror reusing, i.e. kfmclient tells running Konqueror instance
        to open new window
        - without focus stealing prevention - no problem
        - with ASN (application startup notification) - ASN is forwarded,
            and because it's newer than the instance's user timestamp,
            it takes precedence
        - without ASN - user timestamp needs to be reset, otherwise it would
            be used, and it's old; moreover this new window mustn't be detected
            as window belonging to already running application, or it wouldn't
            be activated - see X11Window::sameAppWindowRoleMatch() for the (rather ugly)
            hack
    - konqueror preloading, i.e. window is created in advance, and kfmclient
        tells this Konqueror instance to show it later
        - without focus stealing prevention - no problem
        - with ASN - ASN is forwarded, and because it's newer than the instance's
            user timestamp, it takes precedence
        - without ASN - user timestamp needs to be reset, otherwise it would
            be used, and it's old; also, creation timestamp is changed to
            the time the instance starts (re-)initializing the window,
            this ensures creation timestamp will still work somewhat even in this case
    - KUniqueApplication - when the window is already visible, and the new instance
        wants it to activate
        - without focus stealing prevention - _NET_ACTIVE_WINDOW - no problem
        - with ASN - ASN is forwarded, and set on the already visible window, KWin
            treats the window as new with that ASN
        - without ASN - _NET_ACTIVE_WINDOW as application request is used,
                and there's no really usable timestamp, only timestamp
                from the time the (new) application instance was started,
                so KWin will activate the window *sigh*
                - the bad thing here is that there's absolutely no chance to recognize
                    the case of starting this KUniqueApp from Konsole (and thus wanting
                    the already visible window to become active) from the case
                    when something started this KUniqueApp without ASN (in which case
                    the already visible window shouldn't become active)
                - the only solution is using ASN for starting applications, at least silent
                    (i.e. without feedback)
    - when one application wants to activate another application's window (e.g. KMail
        activating already running KAddressBook window ?)
        - without focus stealing prevention - _NET_ACTIVE_WINDOW - no problem
        - with ASN - can't be here, it's the KUniqueApp case then
        - without ASN - _NET_ACTIVE_WINDOW as application request should be used,
            KWin will activate the new window depending on the timestamp and
            whether it belongs to the currently active application

 _NET_ACTIVE_WINDOW usage:
 data.l[0]= 1 ->app request
          = 2 ->pager request
          = 0 - backwards compatibility
 data.l[1]= timestamp
*/

//****************************************
// Workspace
//****************************************

/**
 * Informs the workspace about the active window, i.e. the window that
 * has the focus (or None if no window has the focus). This functions
 * is called by the window itself that gets focus. It has no other
 * effect than fixing the focus chain and the return value of
 * activeWindow(). And of course, to propagate the active window to the
 * world.
 */
void Workspace::setActiveWindow(Window *window)
{
    if (m_activeWindow == window) {
        return;
    }

    if (active_popup && m_activePopupWindow != window && m_setActiveWindowRecursion == 0) {
        closeActivePopup();
    }
    if (m_userActionsMenu->hasWindow() && !m_userActionsMenu->isMenuWindow(window) && m_setActiveWindowRecursion == 0) {
        m_userActionsMenu->close();
    }
    StackingUpdatesBlocker blocker(this);
    ++m_setActiveWindowRecursion;
    updateFocusMousePosition(Cursors::self()->mouse()->pos());
    if (m_activeWindow != nullptr) {
        // note that this may call setActiveWindow( NULL ), therefore the recursion counter
        m_activeWindow->setActive(false);
    }
    m_activeWindow = window;
    Q_ASSERT(window == nullptr || window->isActive());

    if (m_activeWindow) {
        m_lastActiveWindow = m_activeWindow;
        m_focusChain->update(m_activeWindow, FocusChain::MakeFirst);
        m_activeWindow->demandAttention(false);

        // activating a client can cause a non active fullscreen window to loose the ActiveLayer status on > 1 screens
        if (outputs().count() > 1) {
            for (auto it = m_allClients.begin(); it != m_allClients.end(); ++it) {
                if (*it != m_activeWindow && (*it)->layer() == ActiveLayer && (*it)->output() == m_activeWindow->output()) {
                    (*it)->updateLayer();
                }
            }
        }
    }

    updateToolWindows(false);
    if (window) {
        disableGlobalShortcutsForClient(window->rules()->checkDisableGlobalShortcuts(false));
    } else {
        disableGlobalShortcutsForClient(false);
    }

    updateStackingOrder(); // e.g. fullscreens have different layer when active/not-active

    if (rootInfo()) {
        rootInfo()->setActiveClient(m_activeWindow);
    }

    Q_EMIT windowActivated(m_activeWindow);
    --m_setActiveWindowRecursion;
}

/**
 * Tries to activate the window \a window. This function performs what you
 * expect when clicking the respective entry in a taskbar: showing and
 * raising the window (this may imply switching to the another virtual
 * desktop) and putting the focus onto it. Once X really gave focus to
 * the window window as requested, the window itself will call
 * setActiveWindow() and the operation is complete. This may not happen
 * with certain focus policies, though.
 *
 * @see setActiveWindow
 * @see requestFocus
 */
void Workspace::activateWindow(Window *window, bool force)
{
    if (window == nullptr) {
        focusToNull();
        setActiveWindow(nullptr);
        return;
    }
    if (window->isDeleted()) {
        return;
    }
    raiseWindow(window);
    if (!window->isOnCurrentDesktop()) {
        ++block_focus;
        switch (options->activationDesktopPolicy()) {
        case Options::ActivationDesktopPolicy::SwitchToOtherDesktop:
            VirtualDesktopManager::self()->setCurrent(window->desktops().constLast());
            break;
        case Options::ActivationDesktopPolicy::BringToCurrentDesktop:
            window->enterDesktop(VirtualDesktopManager::self()->currentDesktop());
            break;
        }
        --block_focus;
    }
#if KWIN_BUILD_ACTIVITIES
    if (!window->isOnCurrentActivity()) {
        ++block_focus;
        // DBUS!
        // first isn't necessarily best, but it's easiest
        m_activities->setCurrent(window->activities().constFirst());
        --block_focus;
    }
#endif
    if (window->isMinimized()) {
        window->unminimize();
    }

    // ensure the window is really visible - could eg. be a hidden utility window, see bug #348083
    window->showClient();

    // TODO force should perhaps allow this only if the window already contains the mouse
    if (options->focusPolicyIsReasonable() || force) {
        requestFocus(window, force);
    }

    // Don't update user time for windows that have focus stealing workaround.
    // As they usually belong to the current active window but fail to provide
    // this information, updating their user time would make the user time
    // of the currently active window old, and reject further activation for it.
    // E.g. typing URL in minicli which will show kio_uiserver dialog (with workaround),
    // and then kdesktop shows dialog about SSL certificate.
    // This needs also avoiding user creation time in X11Window::readUserTimeMapTimestamp().
    if (X11Window *x11Window = dynamic_cast<X11Window *>(window)) {
        // updateUserTime is X11 specific
        x11Window->updateUserTime();
    }
}

/**
 * Tries to activate the window by asking X for the input focus. This
 * function does not perform any show, raise or desktop switching. See
 * Workspace::activateWindow() instead.
 *
 * @see activateWindow
 */
bool Workspace::requestFocus(Window *window, bool force)
{
    return takeActivity(window, force ? ActivityFocusForce : ActivityFocus);
}

bool Workspace::takeActivity(Window *window, ActivityFlags flags)
{
    // the 'if ( window == m_activeWindow ) return;' optimization mustn't be done here
    if (!focusChangeEnabled() && (window != m_activeWindow)) {
        flags &= ~ActivityFocus;
    }

    if (!window) {
        focusToNull();
        return true;
    }

    if (flags & ActivityFocus) {
        Window *modal = window->findModal();
        if (modal != nullptr && modal != window) {
            if (modal->desktops() != window->desktops()) {
                modal->setDesktops(window->desktops());
            }
            if (!modal->isShown() && !modal->isMinimized()) { // forced desktop or utility window
                activateWindow(modal); // activating a minimized blocked window will unminimize its modal implicitly
            }
            // if the click was inside the window (i.e. handled is set),
            // but it has a modal, there's no need to use handled mode, because
            // the modal doesn't get the click anyway
            // raising of the original window needs to be still done
            if (flags & ActivityRaise) {
                raiseWindow(window);
            }
            window = modal;
        }
        cancelDelayFocus();
    }
    if (!flags.testFlag(ActivityFocusForce) && (window->isDock() || window->isSplash())) {
        // toplevel menus and dock windows don't take focus if not forced
        // and don't have a flag that they take focus
        if (!window->dockWantsInput()) {
            flags &= ~ActivityFocus;
        }
    }
    if (window->isShade()) {
        if (window->wantsInput() && (flags & ActivityFocus)) {
            // window cannot accept focus, but at least the window should be active (window menu, et. al. )
            window->setActive(true);
            focusToNull();
        }
        flags &= ~ActivityFocus;
    }
    if (!window->isShown()) { // shouldn't happen, call activateWindow() if needed
        qCWarning(KWIN_CORE) << "takeActivity: not shown";
        return false;
    }

    bool ret = true;

    if (flags & ActivityFocus) {
        ret &= window->takeFocus();
    }
    if (flags & ActivityRaise) {
        workspace()->raiseWindow(window);
    }

    if (!window->isOnActiveOutput()) {
        setActiveOutput(window->output());
    }

    return ret;
}

/**
 * Informs the workspace that the window \a window has been hidden. If it
 * was the active window (or to-become the active window),
 * the workspace activates another one.
 *
 * @note @p c may already be destroyed.
 */
void Workspace::windowHidden(Window *window)
{
    Q_ASSERT(!window->isShown() || !window->isOnCurrentDesktop() || !window->isOnCurrentActivity());
    activateNextWindow(window);
}

Window *Workspace::windowUnderMouse(Output *output) const
{
    auto it = stackingOrder().constEnd();
    while (it != stackingOrder().constBegin()) {
        auto window = *(--it);
        if (!window->isClient()) {
            continue;
        }

        // rule out windows which are not really visible.
        // the screen test is rather superfluous for xrandr & twinview since the geometry would differ -> TODO: might be dropped
        if (!(window->isShown() && window->isOnCurrentDesktop() && window->isOnCurrentActivity() && window->isOnOutput(output) && !window->isShade())) {
            continue;
        }

        if (window->frameGeometry().contains(Cursors::self()->mouse()->pos())) {
            return window;
        }
    }
    return nullptr;
}

// deactivates 'window' and activates next window
bool Workspace::activateNextWindow(Window *window)
{
    // if 'c' is not the active or the to-become active one, do nothing
    if (!(window == m_activeWindow || (should_get_focus.count() > 0 && window == should_get_focus.last()))) {
        return false;
    }

    closeActivePopup();

    if (window != nullptr) {
        if (window == m_activeWindow) {
            setActiveWindow(nullptr);
        }
        should_get_focus.removeAll(window);
    }

    // if blocking focus, move focus to the desktop later if needed
    // in order to avoid flickering
    if (!focusChangeEnabled()) {
        focusToNull();
        return true;
    }

    if (!options->focusPolicyIsReasonable()) {
        return false;
    }

    Window *focusCandidate = nullptr;

    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();

    if (!focusCandidate && showingDesktop()) {
        focusCandidate = findDesktop(true, desktop); // to not break the state
    }

    if (!focusCandidate && options->isNextFocusPrefersMouse()) {
        focusCandidate = windowUnderMouse(window ? window->output() : workspace()->activeOutput());
        if (focusCandidate && (focusCandidate == window || focusCandidate->isDesktop())) {
            // should rather not happen, but it cannot get the focus. rest of usability is tested above
            focusCandidate = nullptr;
        }
    }

    if (!focusCandidate) { // no suitable window under the mouse -> find sth. else
        // first try to pass the focus to the (former) active clients leader
        if (window && window->isTransient()) {
            auto leaders = window->mainWindows();
            if (leaders.count() == 1 && m_focusChain->isUsableFocusCandidate(leaders.at(0), window)) {
                focusCandidate = leaders.at(0);
                raiseWindow(focusCandidate); // also raise - we don't know where it came from
            }
        }
        if (!focusCandidate) {
            // nope, ask the focus chain for the next candidate
            focusCandidate = m_focusChain->nextForDesktop(window, desktop);
        }
    }

    if (focusCandidate == nullptr) { // last chance: focus the desktop
        focusCandidate = findDesktop(true, desktop);
    }

    if (focusCandidate != nullptr) {
        requestFocus(focusCandidate);
    } else {
        focusToNull();
    }

    return true;
}

void Workspace::switchToOutput(Output *output)
{
    if (!options->focusPolicyIsReasonable()) {
        return;
    }
    closeActivePopup();
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();
    Window *get_focus = m_focusChain->getForActivation(desktop, output);
    if (get_focus == nullptr) {
        get_focus = findDesktop(true, desktop);
    }
    if (get_focus != nullptr && get_focus != mostRecentlyActivatedWindow()) {
        requestFocus(get_focus);
    }
    setActiveOutput(output);
}

void Workspace::gotFocusIn(const Window *window)
{
    if (should_get_focus.contains(const_cast<Window *>(window))) {
        // remove also all sooner elements that should have got FocusIn,
        // but didn't for some reason (and also won't anymore, because they were sooner)
        while (should_get_focus.first() != window) {
            should_get_focus.pop_front();
        }
        should_get_focus.pop_front(); // remove 'window'
    }
}

void Workspace::setShouldGetFocus(Window *window)
{
    should_get_focus.append(window);
    updateStackingOrder(); // e.g. fullscreens have different layer when active/not-active
}

// basically the same like allowWindowActivation(), this time allowing
// a window to be fully raised upon its own request (XRaiseWindow),
// if refused, it will be raised only on top of windows belonging
// to the same application
bool Workspace::allowFullClientRaising(const KWin::Window *window, xcb_timestamp_t time)
{
    int level = window->rules()->checkFSP(options->focusStealingPreventionLevel());
    if (sessionManager()->state() == SessionState::Saving && level <= 2) { // <= normal
        return true;
    }
    Window *ac = mostRecentlyActivatedWindow();
    if (level == 0) { // none
        return true;
    }
    if (level == 4) { // extreme
        return false;
    }
    if (ac == nullptr || ac->isDesktop()) {
        qCDebug(KWIN_CORE) << "Raising: No window active, allowing";
        return true; // no active window -> always allow
    }
    // TODO window urgency  -> return true?
    if (Window::belongToSameApplication(window, ac, Window::SameApplicationCheck::RelaxedForActive)) {
        qCDebug(KWIN_CORE) << "Raising: Belongs to active application";
        return true;
    }
    if (level == 3) { // high
        return false;
    }
    xcb_timestamp_t user_time = ac->userTime();
    qCDebug(KWIN_CORE) << "Raising, compared:" << time << ":" << user_time
                       << ":" << (NET::timestampCompare(time, user_time) >= 0);
    return NET::timestampCompare(time, user_time) >= 0; // time >= user_time
}

/**
 * Called from X11Window after FocusIn that wasn't initiated by KWin and the window wasn't
 * allowed to activate.
 *
 * Returns @c true if the focus has been restored successfully; otherwise returns @c false.
 */
bool Workspace::restoreFocus()
{
    // this updateXTime() is necessary - as FocusIn events don't have
    // a timestamp *sigh*, kwin's timestamp would be older than the timestamp
    // that was used by whoever caused the focus change, and therefore
    // the attempt to restore the focus would fail due to old timestamp
    kwinApp()->updateXTime();
    if (should_get_focus.count() > 0) {
        return requestFocus(should_get_focus.last());
    } else if (m_lastActiveWindow) {
        return requestFocus(m_lastActiveWindow);
    }
    return true;
}

void Workspace::windowAttentionChanged(Window *window, bool set)
{
    if (set) {
        attention_chain.removeAll(window);
        attention_chain.prepend(window);
    } else {
        attention_chain.removeAll(window);
    }
    Q_EMIT windowDemandsAttentionChanged(window, set);
}

//********************************************
// Client
//********************************************

/**
 * Updates the user time (time of last action in the active window).
 * This is called inside  kwin for every action with the window
 * that qualifies for user interaction (clicking on it, activate it
 * externally, etc.).
 */
void X11Window::updateUserTime(xcb_timestamp_t time)
{
    // copied in Group::updateUserTime
    if (time == XCB_TIME_CURRENT_TIME) {
        kwinApp()->updateXTime();
        time = xTime();
    }
    if (time != -1U
        && (m_userTime == XCB_TIME_CURRENT_TIME
            || NET::timestampCompare(time, m_userTime) > 0)) { // time > user_time
        m_userTime = time;
        shade_below = nullptr; // do not hover re-shade a window after it got interaction
    }
    group()->updateUserTime(m_userTime);
}

xcb_timestamp_t X11Window::readUserCreationTime() const
{
    Xcb::Property prop(false, window(), atoms->kde_net_wm_user_creation_time, XCB_ATOM_CARDINAL, 0, 1);
    return prop.value<xcb_timestamp_t>(-1);
}

xcb_timestamp_t X11Window::readUserTimeMapTimestamp(const KStartupInfoId *asn_id, const KStartupInfoData *asn_data,
                                                    bool session) const
{
    xcb_timestamp_t time = info->userTime();
    // qDebug() << "User timestamp, initial:" << time;
    //^^ this deadlocks kwin --replace sometimes.

    // newer ASN timestamp always replaces user timestamp, unless user timestamp is 0
    // helps e.g. with konqy reusing
    if (asn_data != nullptr && time != 0) {
        if (asn_id->timestamp() != 0
            && (time == -1U || NET::timestampCompare(asn_id->timestamp(), time) > 0)) {
            time = asn_id->timestamp();
        }
    }
    qCDebug(KWIN_CORE) << "User timestamp, ASN:" << time;
    if (time == -1U) {
        // The window doesn't have any timestamp.
        // If it's the first window for its application
        // (i.e. there's no other window from the same app),
        // use the _KDE_NET_WM_USER_CREATION_TIME trick.
        // Otherwise, refuse activation of a window
        // from already running application if this application
        // is not the active one (unless focus stealing prevention is turned off).
        X11Window *act = dynamic_cast<X11Window *>(workspace()->mostRecentlyActivatedWindow());
        if (act != nullptr && !belongToSameApplication(act, this, SameApplicationCheck::RelaxedForActive)) {
            bool first_window = true;
            auto sameApplicationActiveHackPredicate = [this](const X11Window *cl) {
                // ignore already existing splashes, toolbars, utilities and menus,
                // as the app may show those before the main window
                return !cl->isSplash() && !cl->isToolbar() && !cl->isUtility() && !cl->isMenu()
                    && cl != this && X11Window::belongToSameApplication(cl, this, SameApplicationCheck::RelaxedForActive);
            };
            if (isTransient()) {
                auto clientMainClients = [this]() {
                    QList<X11Window *> ret;
                    const auto mcs = mainWindows();
                    for (auto mc : mcs) {
                        if (X11Window *c = dynamic_cast<X11Window *>(mc)) {
                            ret << c;
                        }
                    }
                    return ret;
                };
                if (act->hasTransient(this, true)) {
                    ; // is transient for currently active window, even though it's not
                    // the same app (e.g. kcookiejar dialog) -> allow activation
                } else if (groupTransient() && findInList<X11Window, X11Window>(clientMainClients(), sameApplicationActiveHackPredicate) == nullptr) {
                    ; // standalone transient
                } else {
                    first_window = false;
                }
            } else {
                if (workspace()->findClient(sameApplicationActiveHackPredicate)) {
                    first_window = false;
                }
            }
            // don't refuse if focus stealing prevention is turned off
            if (!first_window && rules()->checkFSP(options->focusStealingPreventionLevel()) > 0) {
                qCDebug(KWIN_CORE) << "User timestamp, already exists:" << 0;
                return 0; // refuse activation
            }
        }
        // Creation time would just mess things up during session startup,
        // as possibly many apps are started up at the same time.
        // If there's no active window yet, no timestamp will be needed,
        // as plain Workspace::allowWindowActivation() will return true
        // in such case. And if there's already active window,
        // it's better not to activate the new one.
        // Unless it was the active window at the time
        // of session saving and there was no user interaction yet,
        // this check will be done in manage().
        if (session) {
            return -1U;
        }
        time = readUserCreationTime();
    }
    qCDebug(KWIN_CORE) << "User timestamp, final:" << this << ":" << time;
    return time;
}

xcb_timestamp_t X11Window::userTime() const
{
    xcb_timestamp_t time = m_userTime;
    if (time == 0) { // doesn't want focus after showing
        return 0;
    }
    Q_ASSERT(group() != nullptr);
    if (time == -1U
        || (group()->userTime() != -1U
            && NET::timestampCompare(group()->userTime(), time) > 0)) {
        time = group()->userTime();
    }
    return time;
}

void X11Window::doSetActive()
{
    updateUrgency(); // demand attention again if it's still urgent
    info->setState(isActive() ? NET::Focused : NET::States(), NET::Focused);
}

void X11Window::startupIdChanged()
{
    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(window(), asn_id, asn_data);
    if (!asn_valid) {
        return;
    }
    // If the ASN contains desktop, move it to the desktop, otherwise move it to the current
    // desktop (since the new ASN should make the window act like if it's a new application
    // launched). However don't affect the window's desktop if it's set to be on all desktops.

    if (asn_data.desktop() != 0 && !isOnAllDesktops()) {
        workspace()->sendWindowToDesktop(this, asn_data.desktop(), true);
    }

    if (asn_data.xinerama() != -1) {
        Output *output = workspace()->xineramaIndexToOutput(asn_data.xinerama());
        if (output) {
            workspace()->sendWindowToOutput(this, output);
        }
    }
    const xcb_timestamp_t timestamp = asn_id.timestamp();
    if (timestamp != 0) {
        bool activate = allowWindowActivation(timestamp);
        if (activate) {
            workspace()->activateWindow(this);
        } else {
            demandAttention();
        }
    }
}

void X11Window::updateUrgency()
{
    if (info->urgency()) {
        demandAttention();
    }
}

namespace FSP
{
enum Level {
    None = 0,
    Low,
    Medium,
    High,
    Extreme,
};
}

// focus_in -> the window got FocusIn event
// ignore_desktop - call comes from _NET_ACTIVE_WINDOW message, don't refuse just because of window
//     is on a different desktop
bool X11Window::allowWindowActivation(xcb_timestamp_t time, bool focus_in, bool ignore_desktop)
{
    auto window = this;
    // options->focusStealingPreventionLevel :
    // 0 - none    - old KWin behaviour, new windows always get focus
    // 1 - low     - focus stealing prevention is applied normally, when unsure, activation is allowed
    // 2 - normal  - focus stealing prevention is applied normally, when unsure, activation is not allowed,
    //              this is the default
    // 3 - high    - new window gets focus only if it belongs to the active application,
    //              or when no window is currently active
    // 4 - extreme - no window gets focus without user intervention
    if (time == -1U) {
        time = window->userTime();
    }
    const FSP::Level level = (FSP::Level)window->rules()->checkFSP(options->focusStealingPreventionLevel());
    if (workspace()->sessionManager()->state() == SessionState::Saving && level <= FSP::Medium) { // <= normal
        return true;
    }
    Window *ac = workspace()->mostRecentlyActivatedWindow();
    if (focus_in) {
        if (workspace()->inShouldGetFocus(window)) {
            return true; // FocusIn was result of KWin's action
        }
        // Before getting FocusIn, the active Client already
        // got FocusOut, and therefore got deactivated.
        ac = workspace()->lastActiveWindow();
    }
    if (time == 0) { // explicitly asked not to get focus
        if (!window->rules()->checkAcceptFocus(false)) {
            return false;
        }
    }
    const FSP::Level protection = (FSP::Level)(ac ? ac->rules()->checkFPP(2) : FSP::None);

    // stealing is unconditionally allowed (NETWM behavior)
    if (level == FSP::None || protection == FSP::None) {
        return true;
    }

    // The active window "grabs" the focus or stealing is generally forbidden
    if (level == FSP::Extreme || protection == FSP::Extreme) {
        return false;
    }

    // Desktop switching is only allowed in the "no protection" case
    if (!ignore_desktop && !window->isOnCurrentDesktop()) {
        return false; // allow only with level == 0
    }

    // No active window, it's ok to pass focus
    // NOTICE that extreme protection needs to be handled before to allow protection on unmanged windows
    if (ac == nullptr || ac->isDesktop()) {
        qCDebug(KWIN_CORE) << "Activation: No window active, allowing";
        return true; // no active window -> always allow
    }

    // TODO window urgency  -> return true?

    // Unconditionally allow intra-window passing around for lower stealing protections
    // unless the active window has High interest
    if (Window::belongToSameApplication(window, ac, Window::SameApplicationCheck::RelaxedForActive) && protection < FSP::High) {
        qCDebug(KWIN_CORE) << "Activation: Belongs to active application";
        return true;
    }

    if (!window->isOnCurrentDesktop()) { // we allowed explicit self-activation across virtual desktops
        return false; // inside a window or if no window was active, but not otherwise
    }

    // High FPS, not intr-window change. Only allow if the active window has only minor interest
    if (level > FSP::Medium && protection > FSP::Low) {
        return false;
    }

    if (time == -1U) { // no time known
        qCDebug(KWIN_CORE) << "Activation: No timestamp at all";
        // Only allow for Low protection unless active window has High interest in focus
        if (level < FSP::Medium && protection < FSP::High) {
            return true;
        }
        // no timestamp at all, don't activate - because there's also creation timestamp
        // done on CreateNotify, this case should happen only in case application
        // maps again already used window, i.e. this won't happen after app startup
        return false;
    }

    // Low or medium FSP, usertime comparism is possible
    const xcb_timestamp_t user_time = ac->userTime();
    qCDebug(KWIN_CORE) << "Activation, compared:" << window << ":" << time << ":" << user_time
                       << ":" << (NET::timestampCompare(time, user_time) >= 0);
    return NET::timestampCompare(time, user_time) >= 0; // time >= user_time
}

//****************************************
// Group
//****************************************

void Group::startupIdChanged()
{
    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(leader_wid, asn_id, asn_data);
    if (!asn_valid) {
        return;
    }
    if (asn_id.timestamp() != 0 && user_time != -1U
        && NET::timestampCompare(asn_id.timestamp(), user_time) > 0) {
        user_time = asn_id.timestamp();
    }
}

void Group::updateUserTime(xcb_timestamp_t time)
{
    // copy of X11Window::updateUserTime
    if (time == XCB_CURRENT_TIME) {
        kwinApp()->updateXTime();
        time = xTime();
    }
    if (time != -1U
        && (user_time == XCB_CURRENT_TIME
            || NET::timestampCompare(time, user_time) > 0)) { // time > user_time
        user_time = time;
    }
}

} // namespace

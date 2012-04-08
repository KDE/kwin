/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
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

/*

 This file contains things relevant to window activation and focus
 stealing prevention.

*/

#include "client.h"
#include "workspace.h"

#include <fixx11h.h>
#include <kxerrorhandler.h>
#include <kstartupinfo.h>
#include <kstringhandler.h>
#include <klocale.h>

#include "notifications.h"
#include "atoms.h"
#include "group.h"
#include "rules.h"
#include <QX11Info>

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
   Workspace::allowClientActivation() (see below).
 - a new window will be mapped - this is the most complicated case. If
   the new window belongs to the currently active application, it may be safely
   mapped on top and activated. The same if there's no active window,
   or the active window is the desktop. These checks are done by
   Workspace::allowClientActivation().
    Following checks need to compare times. One time is the timestamp
   of last user action in the currently active window, the other time is
   the timestamp of the action that originally caused mapping of the new window
   (e.g. when the application was started). If the first time is newer than
   the second one, the window will not be activated, as that indicates
   futher user actions took place after the action leading to this new
   mapped window. This check is done by Workspace::allowClientActivation().
    There are several ways how to get the timestamp of action that caused
   the new mapped window (done in Client::readUserTimeMapTimestamp()) :
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
   process) is done in Client::belongToSameApplication(). Not 100% reliable,
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

 There are one more ways how a window can become obstrusive, window stealing
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
            be activated - see Client::sameAppWindowRoleMatch() for the (rather ugly)
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


/*!
  Informs the workspace about the active client, i.e. the client that
  has the focus (or None if no client has the focus). This functions
  is called by the client itself that gets focus. It has no other
  effect than fixing the focus chain and the return value of
  activeClient(). And of course, to propagate the active client to the
  world.
 */
void Workspace::setActiveClient(Client* c, allowed_t)
{
    if (active_client == c)
        return;

    if (active_popup && active_popup_client != c && set_active_client_recursion == 0)
        closeActivePopup();
    StackingUpdatesBlocker blocker(this);
    ++set_active_client_recursion;
    updateFocusMousePosition(cursorPos());
    if (active_client != NULL) {
        // note that this may call setActiveClient( NULL ), therefore the recursion counter
        active_client->setActive(false);
    }
    active_client = c;
    Q_ASSERT(c == NULL || c->isActive());
    if (active_client != NULL)
        last_active_client = active_client;
    if (active_client) {
        updateFocusChains(active_client, FocusChainMakeFirst);
        active_client->demandAttention(false);
    }
    pending_take_activity = NULL;

    updateToolWindows(false);
    if (c)
        disableGlobalShortcutsForClient(c->rules()->checkDisableGlobalShortcuts(false));
    else
        disableGlobalShortcutsForClient(false);

    updateStackingOrder(); // e.g. fullscreens have different layer when active/not-active

    rootInfo->setActiveWindow(active_client ? active_client->window() : 0);
    updateColormap();

    emit clientActivated(active_client);
    --set_active_client_recursion;
}

/*!
  Tries to activate the client \a c. This function performs what you
  expect when clicking the respective entry in a taskbar: showing and
  raising the client (this may imply switching to the another virtual
  desktop) and putting the focus onto it. Once X really gave focus to
  the client window as requested, the client itself will call
  setActiveClient() and the operation is complete. This may not happen
  with certain focus policies, though.

  \sa stActiveClient(), requestFocus()
 */
void Workspace::activateClient(Client* c, bool force)
{
    if (c == NULL) {
        focusToNull();
        setActiveClient(NULL, Allowed);
        return;
    }
    raiseClient(c);
    if (!c->isOnCurrentDesktop()) {
        ++block_focus;
        setCurrentDesktop(c->desktop());
        --block_focus;
    }
#ifdef KWIN_BUILD_ACTIVITIES
    if (!c->isOnCurrentActivity()) {
        ++block_focus;
        //DBUS!
        activityController_.setCurrentActivity(c->activities().first());   //first isn't necessarily best, but it's easiest
        --block_focus;
    }
#endif
    if (c->isMinimized())
        c->unminimize();

// TODO force should perhaps allow this only if the window already contains the mouse
    if (options->focusPolicyIsReasonable() || force)
        requestFocus(c, force);

    // Don't update user time for clients that have focus stealing workaround.
    // As they usually belong to the current active window but fail to provide
    // this information, updating their user time would make the user time
    // of the currently active window old, and reject further activation for it.
    // E.g. typing URL in minicli which will show kio_uiserver dialog (with workaround),
    // and then kdesktop shows dialog about SSL certificate.
    // This needs also avoiding user creation time in Client::readUserTimeMapTimestamp().
    c->updateUserTime();
}

/*!
  Tries to activate the client by asking X for the input focus. This
  function does not perform any show, raise or desktop switching. See
  Workspace::activateClient() instead.

  \sa Workspace::activateClient()
 */
void Workspace::requestFocus(Client* c, bool force)
{
    takeActivity(c, ActivityFocus | (force ? ActivityFocusForce : 0), false);
}

void Workspace::takeActivity(Client* c, int flags, bool handled)
{
    // the 'if ( c == active_client ) return;' optimization mustn't be done here
    if (!focusChangeEnabled() && (c != active_client))
        flags &= ~ActivityFocus;

    if (!c) {
        focusToNull();
        return;
    }

    if (flags & ActivityFocus) {
        Client* modal = c->findModal();
        if (modal != NULL && modal != c) {
            if (!modal->isOnDesktop(c->desktop())) {
                modal->setDesktop(c->desktop());
                if (modal->desktop() != c->desktop())  // forced desktop
                    activateClient(modal);
            }
            // if the click was inside the window (i.e. handled is set),
            // but it has a modal, there's no need to use handled mode, because
            // the modal doesn't get the click anyway
            // raising of the original window needs to be still done
            if (flags & ActivityRaise)
                raiseClient(c);
            c = modal;
            handled = false;
        }
        cancelDelayFocus();
    }
    if (!(flags & ActivityFocusForce) && (c->isDock() || c->isSplash()))
        flags &= ~ActivityFocus; // toplevel menus and dock windows don't take focus if not forced
    if (c->isShade()) {
        if (c->wantsInput() && (flags & ActivityFocus)) {
            // client cannot accept focus, but at least the window should be active (window menu, et. al. )
            c->setActive(true);
            focusToNull();
        }
        flags &= ~ActivityFocus;
        handled = false; // no point, can't get clicks
    }
    if (c->tabGroup() && c->tabGroup()->current() != c)
        c->tabGroup()->setCurrent(c);
    if (!c->isShown(true)) {  // shouldn't happen, call activateClient() if needed
        kWarning(1212) << "takeActivity: not shown" ;
        return;
    }
    c->takeActivity(flags, handled, Allowed);
    if (!c->isOnScreen(active_screen))
        active_screen = c->screen();
}

void Workspace::handleTakeActivity(Client* c, Time /*timestamp*/, int flags)
{
    if (pending_take_activity != c)   // pending_take_activity is reset when doing restack or activation
        return;
    if ((flags & ActivityRaise) != 0)
        raiseClient(c);
    if ((flags & ActivityFocus) != 0 && c->isShown(false))
        c->takeFocus(Allowed);
    pending_take_activity = NULL;
}

/*!
  Informs the workspace that the client \a c has been hidden. If it
  was the active client (or to-become the active client),
  the workspace activates another one.

  \a c may already be destroyed
 */
void Workspace::clientHidden(Client* c)
{
    assert(!c->isShown(true) || !c->isOnCurrentDesktop() || !c->isOnCurrentActivity());
    activateNextClient(c);
}

static inline bool isUsableFocusCandidate(Client *c, Client *prev, bool respectScreen)
{
    return c != prev &&
           c->isShown(false) && c->isOnCurrentDesktop() && c->isOnCurrentActivity() &&
           (!respectScreen || c->isOnScreen(prev ? prev->screen() : Workspace::self()->activeScreen()));
}

Client *Workspace::clientUnderMouse(int screen) const
{
    ToplevelList::const_iterator it = stackingOrder().constEnd();
    while (it != stackingOrder().constBegin()) {
        Client *client = qobject_cast<Client*>(*(--it));
        if (!client) {
            continue;
        }

        // rule out clients which are not really visible.
        // the screen test is rather superflous for xrandr & twinview since the geometry would differ -> TODO: might be dropped
        if (!(client->isShown(false) && client->isOnCurrentDesktop() &&
                client->isOnCurrentActivity() && client->isOnScreen(screen)))
            continue;

        if (client->geometry().contains(QCursor::pos())) {
            return client;
        }
    }
    return 0;
}

// deactivates 'c' and activates next client
bool Workspace::activateNextClient(Client* c)
{
    // if 'c' is not the active or the to-become active one, do nothing
    if (!(c == active_client || (should_get_focus.count() > 0 && c == should_get_focus.last())))
        return false;

    closeActivePopup();

    if (c != NULL) {
        if (c == active_client)
            setActiveClient(NULL, Allowed);
        should_get_focus.removeAll(c);
    }

    // if blocking focus, move focus to the desktop later if needed
    // in order to avoid flickering
    if (!focusChangeEnabled()) {
        focusToNull();
        return true;
    }

    if (!options->focusPolicyIsReasonable())
        return false;

    Client* get_focus = NULL;

    if (options->isNextFocusPrefersMouse()) {
        get_focus = clientUnderMouse(c ? c->screen() : activeScreen());
        if (get_focus && (get_focus == c || get_focus->isDesktop())) {
            // should rather not happen, but it cannot get the focus. rest of usability is tested above
            get_focus = 0;
        }
    }

    if (!get_focus) { // no suitable window under the mouse -> find sth. else

        // first try to pass the focus to the (former) active clients leader
        if (c  && (get_focus = c->transientFor()) && isUsableFocusCandidate(get_focus, c, options->isSeparateScreenFocus())) {
            raiseClient(get_focus);   // also raise - we don't know where it came from
        } else {
            // nope, ask the focus chain for the next candidate
            get_focus = NULL; // reset from the inline assignment above
            for (int i = focus_chain[ currentDesktop()].size() - 1; i >= 0; --i) {
                Client* ci = focus_chain[ currentDesktop()].at(i);
                if (isUsableFocusCandidate(ci, c, options->isSeparateScreenFocus())) {
                    get_focus = ci;
                    break; // we're done
                }
            }
        }
    }

    if (get_focus == NULL)   // last chance: focus the desktop
        get_focus = findDesktop(true, currentDesktop());

    if (get_focus != NULL)
        requestFocus(get_focus);
    else
        focusToNull();

    return true;

}

void Workspace::setCurrentScreen(int new_screen)
{
    if (new_screen < 0 || new_screen >= numScreens())
        return;
    if (!options->focusPolicyIsReasonable())
        return;
    closeActivePopup();
    Client* get_focus = NULL;
    for (int i = focus_chain[ currentDesktop()].count() - 1;
            i >= 0;
            --i) {
        Client* ci = focus_chain[ currentDesktop()].at(i);
        if (!ci->isShown(false) || !ci->isOnCurrentDesktop() || !ci->isOnCurrentActivity())
            continue;
        if (!ci->screen() == new_screen)
            continue;
        get_focus = ci;
        break;
    }
    if (get_focus == NULL)
        get_focus = findDesktop(true, currentDesktop());
    if (get_focus != NULL && get_focus != mostRecentlyActivatedClient())
        requestFocus(get_focus);
    active_screen = new_screen;
}

void Workspace::gotFocusIn(const Client* c)
{
    if (should_get_focus.contains(const_cast< Client* >(c))) {
        // remove also all sooner elements that should have got FocusIn,
        // but didn't for some reason (and also won't anymore, because they were sooner)
        while (should_get_focus.first() != c)
            should_get_focus.pop_front();
        should_get_focus.pop_front(); // remove 'c'
    }
}

void Workspace::setShouldGetFocus(Client* c)
{
    should_get_focus.append(c);
    updateStackingOrder(); // e.g. fullscreens have different layer when active/not-active
}

// focus_in -> the window got FocusIn event
// ignore_desktop - call comes from _NET_ACTIVE_WINDOW message, don't refuse just because of window
//     is on a different desktop
bool Workspace::allowClientActivation(const Client* c, Time time, bool focus_in, bool ignore_desktop)
{
    // options->focusStealingPreventionLevel :
    // 0 - none    - old KWin behaviour, new windows always get focus
    // 1 - low     - focus stealing prevention is applied normally, when unsure, activation is allowed
    // 2 - normal  - focus stealing prevention is applied normally, when unsure, activation is not allowed,
    //              this is the default
    // 3 - high    - new window gets focus only if it belongs to the active application,
    //              or when no window is currently active
    // 4 - extreme - no window gets focus without user intervention
    if (time == -1U)
        time = c->userTime();
    int level = c->rules()->checkFSP(options->focusStealingPreventionLevel());
    if (session_saving && level <= 2) { // <= normal
        return true;
    }
    Client* ac = mostRecentlyActivatedClient();
    if (focus_in) {
        if (should_get_focus.contains(const_cast< Client* >(c)))
            return true; // FocusIn was result of KWin's action
        // Before getting FocusIn, the active Client already
        // got FocusOut, and therefore got deactivated.
        ac = last_active_client;
    }
    if (time == 0)   // explicitly asked not to get focus
        return false;
    if (level == 0)   // none
        return true;
    if (level == 4)   // extreme
        return false;
    if (!ignore_desktop && !c->isOnCurrentDesktop())
        return false; // allow only with level == 0
    if (ac == NULL || ac->isDesktop()) {
        kDebug(1212) << "Activation: No client active, allowing";
        return true; // no active client -> always allow
    }
    // TODO window urgency  -> return true?
    if (Client::belongToSameApplication(c, ac, true)) {
        kDebug(1212) << "Activation: Belongs to active application";
        return true;
    }
    if (level == 3)   // high
        return false;
    if (time == -1U) {  // no time known
        kDebug(1212) << "Activation: No timestamp at all";
        if (level == 1)   // low
            return true;
        // no timestamp at all, don't activate - because there's also creation timestamp
        // done on CreateNotify, this case should happen only in case application
        // maps again already used window, i.e. this won't happen after app startup
        return false;
    }
    // level == 2 // normal
    Time user_time = ac->userTime();
    kDebug(1212) << "Activation, compared:" << c << ":" << time << ":" << user_time
                 << ":" << (timestampCompare(time, user_time) >= 0) << endl;
    return timestampCompare(time, user_time) >= 0;   // time >= user_time
}

// basically the same like allowClientActivation(), this time allowing
// a window to be fully raised upon its own request (XRaiseWindow),
// if refused, it will be raised only on top of windows belonging
// to the same application
bool Workspace::allowFullClientRaising(const Client* c, Time time)
{
    int level = c->rules()->checkFSP(options->focusStealingPreventionLevel());
    if (session_saving && level <= 2) { // <= normal
        return true;
    }
    Client* ac = mostRecentlyActivatedClient();
    if (level == 0)   // none
        return true;
    if (level == 4)   // extreme
        return false;
    if (ac == NULL || ac->isDesktop()) {
        kDebug(1212) << "Raising: No client active, allowing";
        return true; // no active client -> always allow
    }
    // TODO window urgency  -> return true?
    if (Client::belongToSameApplication(c, ac, true)) {
        kDebug(1212) << "Raising: Belongs to active application";
        return true;
    }
    if (level == 3)   // high
        return false;
    Time user_time = ac->userTime();
    kDebug(1212) << "Raising, compared:" << time << ":" << user_time
                 << ":" << (timestampCompare(time, user_time) >= 0) << endl;
    return timestampCompare(time, user_time) >= 0;   // time >= user_time
}

// called from Client after FocusIn that wasn't initiated by KWin and the client
// wasn't allowed to activate
void Workspace::restoreFocus()
{
    // this updateXTime() is necessary - as FocusIn events don't have
    // a timestamp *sigh*, kwin's timestamp would be older than the timestamp
    // that was used by whoever caused the focus change, and therefore
    // the attempt to restore the focus would fail due to old timestamp
    updateXTime();
    if (should_get_focus.count() > 0)
        requestFocus(should_get_focus.last());
    else if (last_active_client)
        requestFocus(last_active_client);
}

void Workspace::clientAttentionChanged(Client* c, bool set)
{
    if (set) {
        attention_chain.removeAll(c);
        attention_chain.prepend(c);
    } else
        attention_chain.removeAll(c);
    emit clientDemandsAttentionChanged(c, set);
}

//********************************************
// Client
//********************************************

/*!
  Updates the user time (time of last action in the active window).
  This is called inside  kwin for every action with the window
  that qualifies for user interaction (clicking on it, activate it
  externally, etc.).
 */
void Client::updateUserTime(Time time)
{
    // copied in Group::updateUserTime
    if (time == CurrentTime)
        time = xTime();
    if (time != -1U
            && (user_time == CurrentTime
                || timestampCompare(time, user_time) > 0)) {    // time > user_time
        user_time = time;
        shade_below = NULL; // do not hover re-shade a window after it got interaction
    }
    group()->updateUserTime(user_time);
}

Time Client::readUserCreationTime() const
{
    long result = -1; // Time == -1 means none
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    KXErrorHandler handler; // ignore errors?
    status = XGetWindowProperty(display(), window(),
                                atoms->kde_net_wm_user_creation_time, 0, 10000, false, XA_CARDINAL,
                                &type, &format, &nitems, &extra, &data);
    if (status  == Success) {
        if (data && nitems > 0)
            result = *((long*) data);
        XFree(data);
    }
    return result;
}

void Client::demandAttention(bool set)
{
    if (isActive())
        set = false;
    if (demands_attention == set)
        return;
    demands_attention = set;
    if (demands_attention) {
        // Demand attention flag is often set right from manage(), when focus stealing prevention
        // steps in. At that time the window has no taskbar entry yet, so KNotify cannot place
        // e.g. the passive popup next to it. So wait up to 1 second for the icon geometry
        // to be set.
        // Delayed call to KNotify also solves the problem of having X server grab in manage(),
        // which may deadlock when KNotify (or KLauncher when launching KNotify) need to access X.

        // Setting the demands attention state needs to be done directly in KWin, because
        // KNotify would try to set it, resulting in a call to KNotify again, etc.

        info->setState(set ? NET::DemandsAttention : 0, NET::DemandsAttention);

        if (demandAttentionKNotifyTimer == NULL) {
            demandAttentionKNotifyTimer = new QTimer(this);
            demandAttentionKNotifyTimer->setSingleShot(true);
            connect(demandAttentionKNotifyTimer, SIGNAL(timeout()), SLOT(demandAttentionKNotify()));
        }
        demandAttentionKNotifyTimer->start(1000);
    } else
        info->setState(set ? NET::DemandsAttention : 0, NET::DemandsAttention);
    workspace()->clientAttentionChanged(this, set);
    emit demandsAttentionChanged();
}

void Client::demandAttentionKNotify()
{
    Notify::Event e = isOnCurrentDesktop() ? Notify::DemandAttentionCurrent : Notify::DemandAttentionOther;
    Notify::raise(e, i18n("Window '%1' demands attention.", KStringHandler::csqueeze(caption())), this);
    demandAttentionKNotifyTimer->stop();
    demandAttentionKNotifyTimer->deleteLater();
    demandAttentionKNotifyTimer = NULL;
}

// TODO I probably shouldn't be lazy here and do it without the macro, so that people can read it
KWIN_COMPARE_PREDICATE(SameApplicationActiveHackPredicate, Client, const Client*,
                       // ignore already existing splashes, toolbars, utilities and menus,
                       // as the app may show those before the main window
                       !cl->isSplash() && !cl->isToolbar() && !cl->isUtility() && !cl->isMenu()
                       && Client::belongToSameApplication(cl, value, true) && cl != value);

Time Client::readUserTimeMapTimestamp(const KStartupInfoId* asn_id, const KStartupInfoData* asn_data,
                                      bool session) const
{
    Time time = info->userTime();
    //kDebug( 1212 ) << "User timestamp, initial:" << time;
    //^^ this deadlocks kwin --replace sometimes.

    // newer ASN timestamp always replaces user timestamp, unless user timestamp is 0
    // helps e.g. with konqy reusing
    if (asn_data != NULL && time != 0) {
        // prefer timestamp from ASN id (timestamp from data is obsolete way)
        if (asn_id->timestamp() != 0
                && (time == -1U || timestampCompare(asn_id->timestamp(), time) > 0)) {
            time = asn_id->timestamp();
        } else if (asn_data->timestamp() != -1U
                  && (time == -1U || timestampCompare(asn_data->timestamp(), time) > 0)) {
            time = asn_data->timestamp();
        }
    }
    kDebug(1212) << "User timestamp, ASN:" << time;
    if (time == -1U) {
        // The window doesn't have any timestamp.
        // If it's the first window for its application
        // (i.e. there's no other window from the same app),
        // use the _KDE_NET_WM_USER_CREATION_TIME trick.
        // Otherwise, refuse activation of a window
        // from already running application if this application
        // is not the active one (unless focus stealing prevention is turned off).
        Client* act = workspace()->mostRecentlyActivatedClient();
        if (act != NULL && !belongToSameApplication(act, this, true)) {
            bool first_window = true;
            if (isTransient()) {
                if (act->hasTransient(this, true))
                    ; // is transient for currently active window, even though it's not
                // the same app (e.g. kcookiejar dialog) -> allow activation
                else if (groupTransient() &&
                        findClientInList(mainClients(), SameApplicationActiveHackPredicate(this)) == NULL)
                    ; // standalone transient
                else
                    first_window = false;
            } else {
                if (workspace()->findClient(SameApplicationActiveHackPredicate(this)))
                    first_window = false;
            }
            // don't refuse if focus stealing prevention is turned off
            if (!first_window && rules()->checkFSP(options->focusStealingPreventionLevel()) > 0) {
                kDebug(1212) << "User timestamp, already exists:" << 0;
                return 0; // refuse activation
            }
        }
        // Creation time would just mess things up during session startup,
        // as possibly many apps are started up at the same time.
        // If there's no active window yet, no timestamp will be needed,
        // as plain Workspace::allowClientActivation() will return true
        // in such case. And if there's already active window,
        // it's better not to activate the new one.
        // Unless it was the active window at the time
        // of session saving and there was no user interaction yet,
        // this check will be done in manage().
        if (session)
            return -1U;
        time = readUserCreationTime();
    }
    kDebug(1212) << "User timestamp, final:" << this << ":" << time;
    return time;
}

Time Client::userTime() const
{
    Time time = user_time;
    if (time == 0)   // doesn't want focus after showing
        return 0;
    assert(group() != NULL);
    if (time == -1U
            || (group()->userTime() != -1U
                && timestampCompare(group()->userTime(), time) > 0))
        time = group()->userTime();
    return time;
}

/*!
  Sets the client's active state to \a act.

  This function does only change the visual appearance of the client,
  it does not change the focus setting. Use
  Workspace::activateClient() or Workspace::requestFocus() instead.

  If a client receives or looses the focus, it calls setActive() on
  its own.

 */
void Client::setActive(bool act)
{
    if (active == act)
        return;
    active = act;
    const int ruledOpacity = active
                             ? rules()->checkOpacityActive(qRound(opacity() * 100.0))
                             : rules()->checkOpacityInactive(qRound(opacity() * 100.0));
    setOpacity(ruledOpacity / 100.0);
    workspace()->setActiveClient(act ? this : NULL, Allowed);

    if (active)
        Notify::raise(Notify::Activate);

    if (!active)
        cancelAutoRaise();

    if (!active && shade_mode == ShadeActivated)
        setShade(ShadeNormal);

    StackingUpdatesBlocker blocker(workspace());
    workspace()->updateClientLayer(this);   // active windows may get different layer
    ClientList mainclients = mainClients();
    for (ClientList::ConstIterator it = mainclients.constBegin();
            it != mainclients.constEnd();
            ++it)
        if ((*it)->isFullScreen())  // fullscreens go high even if their transient is active
            workspace()->updateClientLayer(*it);
    if (decoration != NULL)
        decoration->activeChange();
    emit activeChanged();
    updateMouseGrab();
    updateUrgency(); // demand attention again if it's still urgent
    workspace()->checkUnredirect();
}

void Client::startupIdChanged()
{
    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(window(), asn_id, asn_data);
    if (!asn_valid)
        return;
    // If the ASN contains desktop, move it to the desktop, otherwise move it to the current
    // desktop (since the new ASN should make the window act like if it's a new application
    // launched). However don't affect the window's desktop if it's set to be on all desktops.
    int desktop = workspace()->currentDesktop();
    if (asn_data.desktop() != 0)
        desktop = asn_data.desktop();
    if (!isOnAllDesktops())
        workspace()->sendClientToDesktop(this, desktop, true);
    if (asn_data.xinerama() != -1)
        workspace()->sendClientToScreen(this, asn_data.xinerama());
    Time timestamp = asn_id.timestamp();
    if (timestamp == 0 && asn_data.timestamp() != -1U)
        timestamp = asn_data.timestamp();
    if (timestamp != 0) {
        bool activate = workspace()->allowClientActivation(this, timestamp);
        if (asn_data.desktop() != 0 && !isOnCurrentDesktop())
            activate = false; // it was started on different desktop than current one
        if (activate)
            workspace()->activateClient(this);
        else
            demandAttention();
    }
}

void Client::updateUrgency()
{
    if (urgency)
        demandAttention();
}

void Client::shortcutActivated()
{
    workspace()->activateClient(this, true);   // force
}

//****************************************
// Group
//****************************************

void Group::startupIdChanged()
{
    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(leader_wid, asn_id, asn_data);
    if (!asn_valid)
        return;
    if (asn_id.timestamp() != 0 && user_time != -1U
            && timestampCompare(asn_id.timestamp(), user_time) > 0) {
        user_time = asn_id.timestamp();
    } else if (asn_data.timestamp() != -1U && user_time != -1U
              && timestampCompare(asn_data.timestamp(), user_time) > 0) {
        user_time = asn_data.timestamp();
    }
}

void Group::updateUserTime(Time time)
{
    // copy of Client::updateUserTime
    if (time == CurrentTime)
        time = xTime();
    if (time != -1U
            && (user_time == CurrentTime
                || timestampCompare(time, user_time) > 0))    // time > user_time
        user_time = time;
}

} // namespace

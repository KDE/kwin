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

 This file contains things relevant to window grouping.

*/

//#define QT_CLEAN_NAMESPACE

#include "group.h"
#include <QTextStream>
#include "workspace.h"
#include "client.h"
#include "effects.h"

#include <assert.h>
#include <kstartupinfo.h>
#include <KWindowSystem>
#include <QDebug>


/*
 TODO
 Rename as many uses of 'transient' as possible (hasTransient->hasSubwindow,etc.),
 or I'll get it backwards in half of the cases again.
*/

namespace KWin
{

//********************************************
// Group
//********************************************

Group::Group(Window leader_P)
    :   leader_client(NULL),
        leader_wid(leader_P),
        leader_info(NULL),
        user_time(-1U),
        refcount(0)
{
    if (leader_P != None) {
        leader_client = workspace()->findClient(Predicate::WindowMatch, leader_P);
        leader_info = new NETWinInfo(connection(), leader_P, rootWindow(),
                                      0, NET::WM2StartupId);
    }
    effect_group = new EffectWindowGroupImpl(this);
    workspace()->addGroup(this);
}

Group::~Group()
{
    delete leader_info;
    delete effect_group;
}

QIcon Group::icon() const
{
    if (leader_client != NULL)
        return leader_client->icon();
    else if (leader_wid != None) {
        QIcon ic;
        NETWinInfo info(connection(), leader_wid, rootWindow(), NET::WMIcon, NET::WM2IconPixmap);
        auto readIcon = [&ic, &info, this](int size, bool scale = true) {
            const QPixmap pix = KWindowSystem::icon(leader_wid, size, size, scale, KWindowSystem::NETWM | KWindowSystem::WMHints, &info);
            if (!pix.isNull()) {
                ic.addPixmap(pix);
            }
        };
        readIcon(16);
        readIcon(32);
        readIcon(48, false);
        readIcon(64, false);
        readIcon(128, false);
        return ic;
    }
    return QIcon();
}

void Group::addMember(Client* member_P)
{
    _members.append(member_P);
//    qDebug() << "GROUPADD:" << this << ":" << member_P;
//    qDebug() << kBacktrace();
}

void Group::removeMember(Client* member_P)
{
//    qDebug() << "GROUPREMOVE:" << this << ":" << member_P;
//    qDebug() << kBacktrace();
    Q_ASSERT(_members.contains(member_P));
    _members.removeAll(member_P);
// there are cases when automatic deleting of groups must be delayed,
// e.g. when removing a member and doing some operation on the possibly
// other members of the group (which would be however deleted already
// if there were no other members)
    if (refcount == 0 && _members.isEmpty()) {
        workspace()->removeGroup(this);
        delete this;
    }
}

void Group::ref()
{
    ++refcount;
}

void Group::deref()
{
    if (--refcount == 0 && _members.isEmpty()) {
        workspace()->removeGroup(this);
        delete this;
    }
}

void Group::gotLeader(Client* leader_P)
{
    assert(leader_P->window() == leader_wid);
    leader_client = leader_P;
}

void Group::lostLeader()
{
    assert(!_members.contains(leader_client));
    leader_client = NULL;
    if (_members.isEmpty()) {
        workspace()->removeGroup(this);
        delete this;
    }
}

//***************************************
// Workspace
//***************************************

Group* Workspace::findGroup(xcb_window_t leader) const
{
    assert(leader != None);
    for (GroupList::ConstIterator it = groups.constBegin();
            it != groups.constEnd();
            ++it)
        if ((*it)->leader() == leader)
            return *it;
    return NULL;
}

// Client is group transient, but has no group set. Try to find
// group with windows with the same client leader.
Group* Workspace::findClientLeaderGroup(const Client* c) const
{
    Group* ret = NULL;
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        if (*it == c)
            continue;
        if ((*it)->wmClientLeader() == c->wmClientLeader()) {
            if (ret == NULL || ret == (*it)->group())
                ret = (*it)->group();
            else {
                // There are already two groups with the same client leader.
                // This most probably means the app uses group transients without
                // setting group for its windows. Merging the two groups is a bad
                // hack, but there's no really good solution for this case.
                ClientList old_group = (*it)->group()->members();
                // old_group autodeletes when being empty
                for (int pos = 0;
                        pos < old_group.count();
                        ++pos) {
                    Client* tmp = old_group[ pos ];
                    if (tmp != c)
                        tmp->changeClientLeaderGroup(ret);
                }
            }
        }
    }
    return ret;
}

void Workspace::updateMinimizedOfTransients(AbstractClient* c)
{
    // if mainwindow is minimized or shaded, minimize transients too
    if (c->isMinimized()) {
        for (auto it = c->transients().constBegin();
                it != c->transients().constEnd();
                ++it) {
            if ((*it)->isModal())
                continue; // there's no reason to hide modal dialogs with the main client
            // but to keep them to eg. watch progress or whatever
            if (!(*it)->isMinimized()) {
                (*it)->minimize();
                updateMinimizedOfTransients((*it));
            }
        }
        if (c->isModal()) { // if a modal dialog is minimized, minimize its mainwindow too
            foreach (AbstractClient * c2, c->mainClients())
            c2->minimize();
        }
    } else {
        // else unmiminize the transients
        for (auto it = c->transients().constBegin();
                it != c->transients().constEnd();
                ++it) {
            if ((*it)->isMinimized()) {
                (*it)->unminimize();
                updateMinimizedOfTransients((*it));
            }
        }
        if (c->isModal()) {
            foreach (AbstractClient * c2, c->mainClients())
            c2->unminimize();
        }
    }
}


/*!
  Sets the client \a c's transient windows' on_all_desktops property to \a on_all_desktops.
 */
void Workspace::updateOnAllDesktopsOfTransients(AbstractClient* c)
{
    for (auto it = c->transients().constBegin();
            it != c->transients().constEnd();
            ++it) {
        if ((*it)->isOnAllDesktops() != c->isOnAllDesktops())
            (*it)->setOnAllDesktops(c->isOnAllDesktops());
    }
}

// A new window has been mapped. Check if it's not a mainwindow for some already existing transient window.
void Workspace::checkTransients(xcb_window_t w)
{
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it)
        (*it)->checkTransient(w);
}


//****************************************
// Toplevel
//****************************************

// hacks for broken apps here
// all resource classes are forced to be lowercase
bool Toplevel::resourceMatch(const Toplevel* c1, const Toplevel* c2)
{
    return c1->resourceClass() == c2->resourceClass();
}


//****************************************
// Client
//****************************************

bool Client::belongToSameApplication(const Client* c1, const Client* c2, SameApplicationChecks checks)
{
    bool same_app = false;

    // tests that definitely mean they belong together
    if (c1 == c2)
        same_app = true;
    else if (c1->isTransient() && c2->hasTransient(c1, true))
        same_app = true; // c1 has c2 as mainwindow
    else if (c2->isTransient() && c1->hasTransient(c2, true))
        same_app = true; // c2 has c1 as mainwindow
    else if (c1->group() == c2->group())
        same_app = true; // same group
    else if (c1->wmClientLeader() == c2->wmClientLeader()
            && c1->wmClientLeader() != c1->window() // if WM_CLIENT_LEADER is not set, it returns window(),
            && c2->wmClientLeader() != c2->window()) // don't use in this test then
        same_app = true; // same client leader

    // tests that mean they most probably don't belong together
    else if ((c1->pid() != c2->pid() && !checks.testFlag(SameApplicationCheck::AllowCrossProcesses))
            || c1->wmClientMachine(false) != c2->wmClientMachine(false))
        ; // different processes
    else if (c1->wmClientLeader() != c2->wmClientLeader()
            && c1->wmClientLeader() != c1->window() // if WM_CLIENT_LEADER is not set, it returns window(),
            && c2->wmClientLeader() != c2->window() // don't use in this test then
            && !checks.testFlag(SameApplicationCheck::AllowCrossProcesses))
        ; // different client leader
    else if (!resourceMatch(c1, c2))
        ; // different apps
    else if (!sameAppWindowRoleMatch(c1, c2, checks.testFlag(SameApplicationCheck::RelaxedForActive))
            && !checks.testFlag(SameApplicationCheck::AllowCrossProcesses))
        ; // "different" apps
    else if (c1->pid() == 0 || c2->pid() == 0)
        ; // old apps that don't have _NET_WM_PID, consider them different
    // if they weren't found to match above
    else
        same_app = true; // looks like it's the same app

    return same_app;
}

// Non-transient windows with window role containing '#' are always
// considered belonging to different applications (unless
// the window role is exactly the same). KMainWindow sets
// window role this way by default, and different KMainWindow
// usually "are" different application from user's point of view.
// This help with no-focus-stealing for e.g. konqy reusing.
// On the other hand, if one of the windows is active, they are
// considered belonging to the same application. This is for
// the cases when opening new mainwindow directly from the application,
// e.g. 'Open New Window' in konqy ( active_hack == true ).
bool Client::sameAppWindowRoleMatch(const Client* c1, const Client* c2, bool active_hack)
{
    if (c1->isTransient()) {
        while (const Client *t = dynamic_cast<const Client*>(c1->transientFor()))
            c1 = t;
        if (c1->groupTransient())
            return c1->group() == c2->group();
#if 0
        // if a group transient is in its own group, it didn't possibly have a group,
        // and therefore should be considered belonging to the same app like
        // all other windows from the same app
        || c1->group()->leaderClient() == c1 || c2->group()->leaderClient() == c2;
#endif
    }
    if (c2->isTransient()) {
        while (const Client *t = dynamic_cast<const Client*>(c2->transientFor()))
            c2 = t;
        if (c2->groupTransient())
            return c1->group() == c2->group();
#if 0
        || c1->group()->leaderClient() == c1 || c2->group()->leaderClient() == c2;
#endif
    }
    int pos1 = c1->windowRole().indexOf('#');
    int pos2 = c2->windowRole().indexOf('#');
    if ((pos1 >= 0 && pos2 >= 0)) {
        if (!active_hack)     // without the active hack for focus stealing prevention,
            return c1 == c2; // different mainwindows are always different apps
        if (!c1->isActive() && !c2->isActive())
            return c1 == c2;
        else
            return true;
    }
    return true;
}

/*

 Transiency stuff: ICCCM 4.1.2.6, NETWM 7.3

 WM_TRANSIENT_FOR is basically means "this is my mainwindow".
 For NET::Unknown windows, transient windows are considered to be NET::Dialog
 windows, for compatibility with non-NETWM clients. KWin may adjust the value
 of this property in some cases (window pointing to itself or creating a loop,
 keeping NET::Splash windows above other windows from the same app, etc.).

 Client::transient_for_id is the value of the WM_TRANSIENT_FOR property, after
 possibly being adjusted by KWin. Client::transient_for points to the Client
 this Client is transient for, or is NULL. If Client::transient_for_id is
 poiting to the root window, the window is considered to be transient
 for the whole window group, as suggested in NETWM 7.3.

 In the case of group transient window, Client::transient_for is NULL,
 and Client::groupTransient() returns true. Such window is treated as
 if it were transient for every window in its window group that has been
 mapped _before_ it (or, to be exact, was added to the same group before it).
 Otherwise two group transients can create loops, which can lead very very
 nasty things (bug #67914 and all its dupes).

 Client::original_transient_for_id is the value of the property, which
 may be different if Client::transient_for_id if e.g. forcing NET::Splash
 to be kept on top of its window group, or when the mainwindow is not mapped
 yet, in which case the window is temporarily made group transient,
 and when the mainwindow is mapped, transiency is re-evaluated.

 This can get a bit complicated with with e.g. two Konqueror windows created
 by the same process. They should ideally appear like two independent applications
 to the user. This should be accomplished by all windows in the same process
 having the same window group (needs to be changed in Qt at the moment), and
 using non-group transients poiting to their relevant mainwindow for toolwindows
 etc. KWin should handle both group and non-group transient dialogs well.

 In other words:
 - non-transient windows     : isTransient() == false
 - normal transients         : transientFor() != NULL
 - group transients          : groupTransient() == true

 - list of mainwindows       : mainClients()  (call once and loop over the result)
 - list of transients        : transients()
 - every window in the group : group()->members()
*/

Xcb::TransientFor Client::fetchTransient() const
{
    return Xcb::TransientFor(window());
}

void Client::readTransientProperty(Xcb::TransientFor &transientFor)
{
    xcb_window_t new_transient_for_id = XCB_WINDOW_NONE;
    if (transientFor.getTransientFor(&new_transient_for_id)) {
        m_originalTransientForId = new_transient_for_id;
        new_transient_for_id = verifyTransientFor(new_transient_for_id, true);
    } else {
        m_originalTransientForId = XCB_WINDOW_NONE;
        new_transient_for_id = verifyTransientFor(XCB_WINDOW_NONE, false);
    }
    setTransient(new_transient_for_id);
}

void Client::readTransient()
{
    Xcb::TransientFor transientFor = fetchTransient();
    readTransientProperty(transientFor);
}

void Client::setTransient(xcb_window_t new_transient_for_id)
{
    if (new_transient_for_id != m_transientForId) {
        removeFromMainClients();
        Client *transient_for = nullptr;
        m_transientForId = new_transient_for_id;
        if (m_transientForId != XCB_WINDOW_NONE && !groupTransient()) {
            transient_for = workspace()->findClient(Predicate::WindowMatch, m_transientForId);
            assert(transient_for != NULL);   // verifyTransient() had to check this
            transient_for->addTransient(this);
        } // checkGroup() will check 'check_active_modal'
        setTransientFor(transient_for);
        checkGroup(NULL, true);   // force, because transiency has changed
        workspace()->updateClientLayer(this);
        workspace()->resetUpdateToolWindowsTimer();
        emit transientChanged();
    }
}

void Client::removeFromMainClients()
{
    if (transientFor())
        transientFor()->removeTransient(this);
    if (groupTransient()) {
        for (ClientList::ConstIterator it = group()->members().constBegin();
                it != group()->members().constEnd();
                ++it)
            (*it)->removeTransient(this);
    }
}

// *sigh* this transiency handling is madness :(
// This one is called when destroying/releasing a window.
// It makes sure this client is removed from all grouping
// related lists.
void Client::cleanGrouping()
{
//    qDebug() << "CLEANGROUPING:" << this;
//    for ( ClientList::ConstIterator it = group()->members().begin();
//         it != group()->members().end();
//         ++it )
//        qDebug() << "CL:" << *it;
//    ClientList mains;
//    mains = mainClients();
//    for ( ClientList::ConstIterator it = mains.begin();
//         it != mains.end();
//         ++it )
//        qDebug() << "MN:" << *it;
    removeFromMainClients();
//    qDebug() << "CLEANGROUPING2:" << this;
//    for ( ClientList::ConstIterator it = group()->members().begin();
//         it != group()->members().end();
//         ++it )
//        qDebug() << "CL2:" << *it;
//    mains = mainClients();
//    for ( ClientList::ConstIterator it = mains.begin();
//         it != mains.end();
//         ++it )
//        qDebug() << "MN2:" << *it;
    for (auto it = transients().constBegin();
            it != transients().constEnd();
       ) {
        if ((*it)->transientFor() == this) {
            removeTransient(*it);
            it = transients().constBegin(); // restart, just in case something more has changed with the list
        } else
            ++it;
    }
//    qDebug() << "CLEANGROUPING3:" << this;
//    for ( ClientList::ConstIterator it = group()->members().begin();
//         it != group()->members().end();
//         ++it )
//        qDebug() << "CL3:" << *it;
//    mains = mainClients();
//    for ( ClientList::ConstIterator it = mains.begin();
//         it != mains.end();
//         ++it )
//        qDebug() << "MN3:" << *it;
    // HACK
    // removeFromMainClients() did remove 'this' from transient
    // lists of all group members, but then made windows that
    // were transient for 'this' group transient, which again
    // added 'this' to those transient lists :(
    ClientList group_members = group()->members();
    group()->removeMember(this);
    in_group = NULL;
    for (ClientList::ConstIterator it = group_members.constBegin();
            it != group_members.constEnd();
            ++it)
        (*it)->removeTransient(this);
//    qDebug() << "CLEANGROUPING4:" << this;
//    for ( ClientList::ConstIterator it = group_members.begin();
//         it != group_members.end();
//         ++it )
//        qDebug() << "CL4:" << *it;
    m_transientForId = XCB_WINDOW_NONE;
}

// Make sure that no group transient is considered transient
// for a window that is (directly or indirectly) transient for it
// (including another group transients).
// Non-group transients not causing loops are checked in verifyTransientFor().
void Client::checkGroupTransients()
{
    for (ClientList::ConstIterator it1 = group()->members().constBegin();
            it1 != group()->members().constEnd();
            ++it1) {
        if (!(*it1)->groupTransient())  // check all group transients in the group
            continue;                  // TODO optimize to check only the changed ones?
        for (ClientList::ConstIterator it2 = group()->members().constBegin();
                it2 != group()->members().constEnd();
                ++it2) { // group transients can be transient only for others in the group,
            // so don't make them transient for the ones that are transient for it
            if (*it1 == *it2)
                continue;
            for (AbstractClient* cl = (*it2)->transientFor();
                    cl != NULL;
                    cl = cl->transientFor()) {
                if (cl == *it1) {
                    // don't use removeTransient(), that would modify *it2 too
                    (*it2)->removeTransientFromList(*it1);
                    continue;
                }
            }
            // if *it1 and *it2 are both group transients, and are transient for each other,
            // make only *it2 transient for *it1 (i.e. subwindow), as *it2 came later,
            // and should be therefore on top of *it1
            // TODO This could possibly be optimized, it also requires hasTransient() to check for loops.
            if ((*it2)->groupTransient() && (*it1)->hasTransient(*it2, true) && (*it2)->hasTransient(*it1, true))
                (*it2)->removeTransientFromList(*it1);
            // if there are already windows W1 and W2, W2 being transient for W1, and group transient W3
            // is added, make it transient only for W2, not for W1, because it's already indirectly
            // transient for it - the indirect transiency actually shouldn't break anything,
            // but it can lead to exponentially expensive operations (#95231)
            // TODO this is pretty slow as well
            for (ClientList::ConstIterator it3 = group()->members().constBegin();
                    it3 != group()->members().constEnd();
                    ++it3) {
                if (*it1 == *it2 || *it2 == *it3 || *it1 == *it3)
                    continue;
                if ((*it2)->hasTransient(*it1, false) && (*it3)->hasTransient(*it1, false)) {
                    if ((*it2)->hasTransient(*it3, true))
                        (*it2)->removeTransientFromList(*it1);
                    if ((*it3)->hasTransient(*it2, true))
                        (*it3)->removeTransientFromList(*it1);
                }
            }
        }
    }
}

/*!
  Check that the window is not transient for itself, and similar nonsense.
 */
xcb_window_t Client::verifyTransientFor(xcb_window_t new_transient_for, bool set)
{
    xcb_window_t new_property_value = new_transient_for;
    // make sure splashscreens are shown above all their app's windows, even though
    // they're in Normal layer
    if (isSplash() && new_transient_for == XCB_WINDOW_NONE)
        new_transient_for = rootWindow();
    if (new_transient_for == XCB_WINDOW_NONE) {
        if (set)   // sometimes WM_TRANSIENT_FOR is set to None, instead of root window
            new_property_value = new_transient_for = rootWindow();
        else
            return XCB_WINDOW_NONE;
    }
    if (new_transient_for == window()) { // pointing to self
        // also fix the property itself
        qCWarning(KWIN_CORE) << "Client " << this << " has WM_TRANSIENT_FOR poiting to itself." ;
        new_property_value = new_transient_for = rootWindow();
    }
//  The transient_for window may be embedded in another application,
//  so kwin cannot see it. Try to find the managed client for the
//  window and fix the transient_for property if possible.
    xcb_window_t before_search = new_transient_for;
    while (new_transient_for != XCB_WINDOW_NONE
            && new_transient_for != rootWindow()
            && !workspace()->findClient(Predicate::WindowMatch, new_transient_for)) {
        Xcb::Tree tree(new_transient_for);
        if (tree.isNull()) {
            break;
        }
        new_transient_for = tree->parent;
    }
    if (Client* new_transient_for_client = workspace()->findClient(Predicate::WindowMatch, new_transient_for)) {
        if (new_transient_for != before_search) {
            qCDebug(KWIN_CORE) << "Client " << this << " has WM_TRANSIENT_FOR poiting to non-toplevel window "
                         << before_search << ", child of " << new_transient_for_client << ", adjusting.";
            new_property_value = new_transient_for; // also fix the property
        }
    } else
        new_transient_for = before_search; // nice try
// loop detection
// group transients cannot cause loops, because they're considered transient only for non-transient
// windows in the group
    int count = 20;
    xcb_window_t loop_pos = new_transient_for;
    while (loop_pos != XCB_WINDOW_NONE && loop_pos != rootWindow()) {
        Client* pos = workspace()->findClient(Predicate::WindowMatch, loop_pos);
        if (pos == NULL)
            break;
        loop_pos = pos->m_transientForId;
        if (--count == 0 || pos == this) {
            qCWarning(KWIN_CORE) << "Client " << this << " caused WM_TRANSIENT_FOR loop." ;
            new_transient_for = rootWindow();
        }
    }
    if (new_transient_for != rootWindow()
            && workspace()->findClient(Predicate::WindowMatch, new_transient_for) == NULL) {
        // it's transient for a specific window, but that window is not mapped
        new_transient_for = rootWindow();
    }
    if (new_property_value != m_originalTransientForId)
        Xcb::setTransientFor(window(), new_property_value);
    return new_transient_for;
}

void Client::addTransient(AbstractClient* cl)
{
    AbstractClient::addTransient(cl);
    if (workspace()->mostRecentlyActivatedClient() == this && cl->isModal())
        check_active_modal = true;
//    qDebug() << "ADDTRANS:" << this << ":" << cl;
//    qDebug() << kBacktrace();
//    for ( ClientList::ConstIterator it = transients_list.begin();
//         it != transients_list.end();
//         ++it )
//        qDebug() << "AT:" << (*it);
}

void Client::removeTransient(AbstractClient* cl)
{
//    qDebug() << "REMOVETRANS:" << this << ":" << cl;
//    qDebug() << kBacktrace();
    // cl is transient for this, but this is going away
    // make cl group transient
    AbstractClient::removeTransient(cl);
    if (cl->transientFor() == this) {
        if (Client *c = dynamic_cast<Client*>(cl)) {
            c->m_transientForId = XCB_WINDOW_NONE;
            c->setTransientFor(nullptr); // SELI
// SELI       cl->setTransient( rootWindow());
            c->setTransient(XCB_WINDOW_NONE);
        }
    }
}

// A new window has been mapped. Check if it's not a mainwindow for this already existing window.
void Client::checkTransient(xcb_window_t w)
{
    if (m_originalTransientForId != w)
        return;
    w = verifyTransientFor(w, true);
    setTransient(w);
}

// returns true if cl is the transient_for window for this client,
// or recursively the transient_for window
bool Client::hasTransient(const AbstractClient* cl, bool indirect) const
{
    if (const Client *c = dynamic_cast<const Client*>(cl)) {
        // checkGroupTransients() uses this to break loops, so hasTransient() must detect them
        ConstClientList set;
        return hasTransientInternal(c, indirect, set);
    }
    return false;
}

bool Client::hasTransientInternal(const Client* cl, bool indirect, ConstClientList& set) const
{
    if (const Client *t = dynamic_cast<const Client*>(cl->transientFor())) {
        if (t == this)
            return true;
        if (!indirect)
            return false;
        if (set.contains(cl))
            return false;
        set.append(cl);
        return hasTransientInternal(t, indirect, set);
    }
    if (!cl->isTransient())
        return false;
    if (group() != cl->group())
        return false;
    // cl is group transient, search from top
    if (transients().contains(const_cast< Client* >(cl)))
        return true;
    if (!indirect)
        return false;
    if (set.contains(this))
        return false;
    set.append(this);
    for (auto it = transients().constBegin();
            it != transients().constEnd();
            ++it) {
        const Client *c = qobject_cast<const Client *>(*it);
        if (!c) {
            continue;
        }
        if (c->hasTransientInternal(cl, indirect, set))
            return true;
    }
    return false;
}

QList<AbstractClient*> Client::mainClients() const
{
    if (!isTransient())
        return QList<AbstractClient*>();
    if (const AbstractClient *t = transientFor())
        return QList<AbstractClient*>{const_cast< AbstractClient* >(t)};
    QList<AbstractClient*> result;
    Q_ASSERT(group());
    for (ClientList::ConstIterator it = group()->members().constBegin();
            it != group()->members().constEnd();
            ++it)
        if ((*it)->hasTransient(this, false))
            result.append(*it);
    return result;
}

AbstractClient* Client::findModal(bool allow_itself)
{
    for (auto it = transients().constBegin();
            it != transients().constEnd();
            ++it)
        if (AbstractClient* ret = (*it)->findModal(true))
            return ret;
    if (isModal() && allow_itself)
        return this;
    return NULL;
}

// Client::window_group only holds the contents of the hint,
// but it should be used only to find the group, not for anything else
// Argument is only when some specific group needs to be set.
void Client::checkGroup(Group* set_group, bool force)
{
    Group* old_group = in_group;
    if (old_group != NULL)
        old_group->ref(); // turn off automatic deleting
    if (set_group != NULL) {
        if (set_group != in_group) {
            if (in_group != NULL)
                in_group->removeMember(this);
            in_group = set_group;
            in_group->addMember(this);
        }
    } else if (info->groupLeader() != XCB_WINDOW_NONE) {
        Group* new_group = workspace()->findGroup(info->groupLeader());
        Client *t = qobject_cast<Client*>(transientFor());
        if (t != NULL && t->group() != new_group) {
            // move the window to the right group (e.g. a dialog provided
            // by different app, but transient for this one, so make it part of that group)
            new_group = t->group();
        }
        if (new_group == NULL)   // doesn't exist yet
            new_group = new Group(info->groupLeader());
        if (new_group != in_group) {
            if (in_group != NULL)
                in_group->removeMember(this);
            in_group = new_group;
            in_group->addMember(this);
        }
    } else {
        if (Client *t = qobject_cast<Client*>(transientFor())) {
            // doesn't have window group set, but is transient for something
            // so make it part of that group
            Group* new_group = t->group();
            if (new_group != in_group) {
                if (in_group != NULL)
                    in_group->removeMember(this);
                in_group = t->group();
                in_group->addMember(this);
            }
        } else if (groupTransient()) {
            // group transient which actually doesn't have a group :(
            // try creating group with other windows with the same client leader
            Group* new_group = workspace()->findClientLeaderGroup(this);
            if (new_group == NULL)
                new_group = new Group(None);
            if (new_group != in_group) {
                if (in_group != NULL)
                    in_group->removeMember(this);
                in_group = new_group;
                in_group->addMember(this);
            }
        } else { // Not transient without a group, put it in its client leader group.
            // This might be stupid if grouping was used for e.g. taskbar grouping
            // or minimizing together the whole group, but as long as it is used
            // only for dialogs it's better to keep windows from one app in one group.
            Group* new_group = workspace()->findClientLeaderGroup(this);
            if (in_group != NULL && in_group != new_group) {
                in_group->removeMember(this);
                in_group = NULL;
            }
            if (new_group == NULL)
                new_group = new Group(None);
            if (in_group != new_group) {
                in_group = new_group;
                in_group->addMember(this);
            }
        }
    }
    if (in_group != old_group || force) {
        for (auto it = transients().constBegin();
                it != transients().constEnd();
           ) {
            Client *c = dynamic_cast<Client *>(*it);
            if (!c) {
                ++it;
                continue;
            }
            // group transients in the old group are no longer transient for it
            if (c->groupTransient() && c->group() != group()) {
                removeTransientFromList(c);
                it = transients().constBegin(); // restart, just in case something more has changed with the list
            } else
                ++it;
        }
        if (groupTransient()) {
            // no longer transient for ones in the old group
            if (old_group != NULL) {
                for (ClientList::ConstIterator it = old_group->members().constBegin();
                        it != old_group->members().constEnd();
                        ++it)
                    (*it)->removeTransient(this);
            }
            // and make transient for all in the new group
            for (ClientList::ConstIterator it = group()->members().constBegin();
                    it != group()->members().constEnd();
                    ++it) {
                if (*it == this)
                    break; // this means the window is only transient for windows mapped before it
                (*it)->addTransient(this);
            }
        }
        // group transient splashscreens should be transient even for windows
        // in group mapped later
        for (ClientList::ConstIterator it = group()->members().constBegin();
                it != group()->members().constEnd();
                ++it) {
            if (!(*it)->isSplash())
                continue;
            if (!(*it)->groupTransient())
                continue;
            if (*it == this || hasTransient(*it, true))    // TODO indirect?
                continue;
            addTransient(*it);
        }
    }
    if (old_group != NULL)
        old_group->deref(); // can be now deleted if empty
    checkGroupTransients();
    checkActiveModal();
    workspace()->updateClientLayer(this);
}

// used by Workspace::findClientLeaderGroup()
void Client::changeClientLeaderGroup(Group* gr)
{
    // transientFor() != NULL are in the group of their mainwindow, so keep them there
    if (transientFor() != NULL)
        return;
    // also don't change the group for window which have group set
    if (info->groupLeader())
        return;
    checkGroup(gr);   // change group
}

bool Client::check_active_modal = false;

void Client::checkActiveModal()
{
    // if the active window got new modal transient, activate it.
    // cannot be done in AddTransient(), because there may temporarily
    // exist loops, breaking findModal
    Client* check_modal = dynamic_cast<Client*>(workspace()->mostRecentlyActivatedClient());
    if (check_modal != NULL && check_modal->check_active_modal) {
        Client* new_modal = dynamic_cast<Client*>(check_modal->findModal());
        if (new_modal != NULL && new_modal != check_modal) {
            if (!new_modal->isManaged())
                return; // postpone check until end of manage()
            workspace()->activateClient(new_modal);
        }
        check_modal->check_active_modal = false;
    }
}

} // namespace

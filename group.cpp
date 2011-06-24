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
#include <QX11Info>


/*
 TODO
 Rename as many uses of 'transient' as possible (hasTransient->hasSubwindow,etc.),
 or I'll get it backwards in half of the cases again.
*/

namespace KWin
{

/*
 Consistency checks for window relations. Since transients are determined
 using Client::transiency_list and main windows are determined using Client::transientFor()
 or the group for group transients, these have to match both ways.
*/
//#define ENABLE_TRANSIENCY_CHECK

#ifdef NDEBUG
#undef ENABLE_TRANSIENCY_CHECK
#endif

#ifdef ENABLE_TRANSIENCY_CHECK
static bool transiencyCheckNonExistent = false;

bool performTransiencyCheck()
{
    bool ret = true;
    ClientList clients = Workspace::self()->clients;
    for (ClientList::ConstIterator it1 = clients.constBegin();
            it1 != clients.constEnd();
            ++it1) {
        if ((*it1)->deleting)
            continue;
        if ((*it1)->in_group == NULL) {
            kDebug(1212) << "TC: " << *it1 << " in not in a group" << endl;
            ret = false;
        } else if (!(*it1)->in_group->members().contains(*it1)) {
            kDebug(1212) << "TC: " << *it1 << " has a group " << (*it1)->in_group << " but group does not contain it" << endl;
            ret = false;
        }
        if (!(*it1)->isTransient()) {
            if (!(*it1)->mainClients().isEmpty()) {
                kDebug(1212) << "TC: " << *it1 << " is not transient, has main clients:" << (*it1)->mainClients() << endl;
                ret = false;
            }
        } else {
            ClientList mains = (*it1)->mainClients();
            for (ClientList::ConstIterator it2 = mains.constBegin();
                    it2 != mains.constEnd();
                    ++it2) {
                if (transiencyCheckNonExistent
                        && !Workspace::self()->clients.contains(*it2)
                        && !Workspace::self()->desktops.contains(*it2)) {
                    kDebug(1212) << "TC:" << *it1 << " has non-existent main client ";
                    kDebug(1212) << "TC2:" << *it2; // this may crash
                    ret = false;
                    continue;
                }
                if (!(*it2)->transients_list.contains(*it1)) {
                    kdDebug(1212) << "TC:" << *it1 << " has main client " << *it2 << " but main client does not have it as a transient" << endl;
                    ret = false;
                }
            }
        }
        ClientList trans = (*it1)->transients_list;
        for (ClientList::ConstIterator it2 = trans.constBegin();
                it2 != trans.constEnd();
                ++it2) {
            if (transiencyCheckNonExistent
                    && !Workspace::self()->clients.contains(*it2)
                    && !Workspace::self()->desktops.contains(*it2)) {
                kDebug(1212) << "TC:" << *it1 << " has non-existent transient ";
                kDebug(1212) << "TC2:" << *it2; // this may crash
                ret = false;
                continue;
            }
            if (!(*it2)->mainClients().contains(*it1)) {
                kdDebug(1212) << "TC:" << *it1 << " has transient " << *it2 << " but transient does not have it as a main client" << endl;
                ret = false;
            }
        }
    }
    GroupList groups = Workspace::self()->groups;
    for (GroupList::ConstIterator it1 = groups.constBegin();
            it1 != groups.constEnd();
            ++it1) {
        ClientList members = (*it1)->members();
        for (ClientList::ConstIterator it2 = members.constBegin();
                it2 != members.constEnd();
                ++it2) {
            if ((*it2)->in_group != *it1) {
                kDebug(1212) << "TC: Group " << *it1 << " contains client " << *it2 << " but client is not in that group" << endl;
                ret = false;
            }
        }
    }
    return ret;
}

static QString transiencyCheckStartBt;
static const Client* transiencyCheckClient;
static int transiencyCheck = 0;

static void startTransiencyCheck(const QString& bt, const Client* c, bool ne)
{
    if (++transiencyCheck == 1) {
        transiencyCheckStartBt = bt;
        transiencyCheckClient = c;
    }
    if (ne)
        transiencyCheckNonExistent = true;
}
static void checkTransiency()
{
    if (--transiencyCheck == 0) {
        if (!performTransiencyCheck()) {
            kDebug(1212) << "BT:" << transiencyCheckStartBt << endl;
            kDebug(1212) << "CLIENT:" << transiencyCheckClient << endl;
            abort();
        }
        transiencyCheckNonExistent = false;
    }
}
class TransiencyChecker
{
public:
    TransiencyChecker(const QString& bt, const Client*c) {
        startTransiencyCheck(bt, c, false);
    }
    ~TransiencyChecker() {
        checkTransiency();
    }
};

void checkNonExistentClients()
{
    startTransiencyCheck(kdBacktrace(), NULL, true);
    checkTransiency();
}

#define TRANSIENCY_CHECK( c ) TransiencyChecker transiency_checker( kdBacktrace(), c )

#else

#define TRANSIENCY_CHECK( c )

void checkNonExistentClients()
{
}

#endif

//********************************************
// Group
//********************************************

Group::Group(Window leader_P, Workspace* workspace_P)
    :   leader_client(NULL),
        leader_wid(leader_P),
        _workspace(workspace_P),
        leader_info(NULL),
        user_time(-1U),
        refcount(0)
{
    if (leader_P != None) {
        leader_client = workspace_P->findClient(WindowMatchPredicate(leader_P));
        unsigned long properties[ 2 ] = { 0, NET::WM2StartupId };
        leader_info = new NETWinInfo2(display(), leader_P, rootWindow(),
                                      properties, 2);
    }
    effect_group = new EffectWindowGroupImpl(this);
    workspace()->addGroup(this, Allowed);
}

Group::~Group()
{
    delete leader_info;
    delete effect_group;
}

QPixmap Group::icon() const
{
    if (leader_client != NULL)
        return leader_client->icon();
    else if (leader_wid != None) {
        QPixmap ic;
        Client::readIcons(leader_wid, &ic, NULL, NULL, NULL);
        return ic;
    }
    return QPixmap();
}

QPixmap Group::miniIcon() const
{
    if (leader_client != NULL)
        return leader_client->miniIcon();
    else if (leader_wid != None) {
        QPixmap ic;
        Client::readIcons(leader_wid, NULL, &ic, NULL, NULL);
        return ic;
    }
    return QPixmap();
}

QPixmap Group::bigIcon() const
{
    if (leader_client != NULL)
        return leader_client->bigIcon();
    else if (leader_wid != None) {
        QPixmap ic;
        Client::readIcons(leader_wid, NULL, NULL, &ic, NULL);
        return ic;
    }
    return QPixmap();
}

QPixmap Group::hugeIcon() const
{
    if (leader_client != NULL)
        return leader_client->hugeIcon();
    else if (leader_wid != None) {
        QPixmap ic;
        Client::readIcons(leader_wid, NULL, NULL, NULL, &ic);
        return ic;
    }
    return QPixmap();
}

void Group::addMember(Client* member_P)
{
    TRANSIENCY_CHECK(member_P);
    _members.append(member_P);
//    kDebug(1212) << "GROUPADD:" << this << ":" << member_P;
//    kDebug(1212) << kBacktrace();
}

void Group::removeMember(Client* member_P)
{
    TRANSIENCY_CHECK(member_P);
//    kDebug(1212) << "GROUPREMOVE:" << this << ":" << member_P;
//    kDebug(1212) << kBacktrace();
    Q_ASSERT(_members.contains(member_P));
    _members.removeAll(member_P);
// there are cases when automatic deleting of groups must be delayed,
// e.g. when removing a member and doing some operation on the possibly
// other members of the group (which would be however deleted already
// if there were no other members)
    if (refcount == 0 && _members.isEmpty()) {
        workspace()->removeGroup(this, Allowed);
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
        workspace()->removeGroup(this, Allowed);
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
        workspace()->removeGroup(this, Allowed);
        delete this;
    }
}

void Group::getIcons()
{
    // TODO - also needs adding the flag to NETWinInfo
}

//***************************************
// Workspace
//***************************************

Group* Workspace::findGroup(Window leader) const
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
    TRANSIENCY_CHECK(c);
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

void Workspace::updateMinimizedOfTransients(Client* c)
{
    // if mainwindow is minimized or shaded, minimize transients too
    if (c->isMinimized()) {
        for (ClientList::ConstIterator it = c->transients().constBegin();
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
            foreach (Client * c2, c->mainClients())
            c2->minimize();
        }
    } else {
        // else unmiminize the transients
        for (ClientList::ConstIterator it = c->transients().constBegin();
                it != c->transients().constEnd();
                ++it) {
            if ((*it)->isMinimized()) {
                (*it)->unminimize();
                updateMinimizedOfTransients((*it));
            }
        }
        if (c->isModal()) {
            foreach (Client * c2, c->mainClients())
            c2->unminimize();
        }
    }
}


/*!
  Sets the client \a c's transient windows' on_all_desktops property to \a on_all_desktops.
 */
void Workspace::updateOnAllDesktopsOfTransients(Client* c)
{
    for (ClientList::ConstIterator it = c->transients().constBegin();
            it != c->transients().constEnd();
            ++it) {
        if ((*it)->isOnAllDesktops() != c->isOnAllDesktops())
            (*it)->setOnAllDesktops(c->isOnAllDesktops());
    }
}

/*!
  Sets the client \a c's transient windows' on_all_activities property to \a on_all_desktops.
 */
void Workspace::updateOnAllActivitiesOfTransients(Client* c)
{
    for (ClientList::ConstIterator it = c->transients().constBegin();
            it != c->transients().constEnd();
            ++it) {
        if ((*it)->isOnAllActivities() != c->isOnAllActivities())
            (*it)->setOnAllActivities(c->isOnAllActivities());
    }
}

// A new window has been mapped. Check if it's not a mainwindow for some already existing transient window.
void Workspace::checkTransients(Window w)
{
    TRANSIENCY_CHECK(NULL);
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
    // xv has "xv" as resource name, and different strings starting with "XV" as resource class
    if (qstrncmp(c1->resourceClass(), "xv", 2) == 0 && c1->resourceName() == "xv")
        return qstrncmp(c2->resourceClass(), "xv", 2) == 0 && c2->resourceName() == "xv";
    // Mozilla has "Mozilla" as resource name, and different strings as resource class
    if (c1->resourceName() == "mozilla")
        return c2->resourceName() == "mozilla";
    return c1->resourceClass() == c2->resourceClass();
}


//****************************************
// Client
//****************************************

bool Client::belongToSameApplication(const Client* c1, const Client* c2, bool active_hack)
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
    else if (c1->pid() != c2->pid()
            || c1->wmClientMachine(false) != c2->wmClientMachine(false))
        ; // different processes
    else if (c1->wmClientLeader() != c2->wmClientLeader()
            && c1->wmClientLeader() != c1->window() // if WM_CLIENT_LEADER is not set, it returns window(),
            && c2->wmClientLeader() != c2->window()) // don't use in this test then
        ; // different client leader
    else if (!resourceMatch(c1, c2))
        ; // different apps
    else if (!sameAppWindowRoleMatch(c1, c2, active_hack))
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
        while (c1->transientFor() != NULL)
            c1 = c1->transientFor();
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
        while (c2->transientFor() != NULL)
            c2 = c2->transientFor();
        if (c2->groupTransient())
            return c1->group() == c2->group();
#if 0
        || c1->group()->leaderClient() == c1 || c2->group()->leaderClient() == c2;
#endif
    }
    int pos1 = c1->windowRole().indexOf('#');
    int pos2 = c2->windowRole().indexOf('#');
    if ((pos1 >= 0 && pos2 >= 0)
            ||
            // hacks here
            // Mozilla has resourceName() and resourceClass() swapped
            (c1->resourceName() == "mozilla" && c2->resourceName() == "mozilla")) {
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

void Client::readTransient()
{
    TRANSIENCY_CHECK(this);
    Window new_transient_for_id;
    if (XGetTransientForHint(display(), window(), &new_transient_for_id)) {
        original_transient_for_id = new_transient_for_id;
        new_transient_for_id = verifyTransientFor(new_transient_for_id, true);
    } else {
        original_transient_for_id = None;
        new_transient_for_id = verifyTransientFor(None, false);
    }
    setTransient(new_transient_for_id);
}

void Client::setTransient(Window new_transient_for_id)
{
    TRANSIENCY_CHECK(this);
    if (new_transient_for_id != transient_for_id) {
        removeFromMainClients();
        transient_for = NULL;
        transient_for_id = new_transient_for_id;
        if (transient_for_id != None && !groupTransient()) {
            transient_for = workspace()->findClient(WindowMatchPredicate(transient_for_id));
            assert(transient_for != NULL);   // verifyTransient() had to check this
            transient_for->addTransient(this);
        } // checkGroup() will check 'check_active_modal'
        checkGroup(NULL, true);   // force, because transiency has changed
        workspace()->updateClientLayer(this);
        workspace()->resetUpdateToolWindowsTimer();
    }
}

void Client::removeFromMainClients()
{
    TRANSIENCY_CHECK(this);
    if (transientFor() != NULL)
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
    TRANSIENCY_CHECK(this);
//    kDebug(1212) << "CLEANGROUPING:" << this;
//    for ( ClientList::ConstIterator it = group()->members().begin();
//         it != group()->members().end();
//         ++it )
//        kDebug(1212) << "CL:" << *it;
//    ClientList mains;
//    mains = mainClients();
//    for ( ClientList::ConstIterator it = mains.begin();
//         it != mains.end();
//         ++it )
//        kDebug(1212) << "MN:" << *it;
    removeFromMainClients();
//    kDebug(1212) << "CLEANGROUPING2:" << this;
//    for ( ClientList::ConstIterator it = group()->members().begin();
//         it != group()->members().end();
//         ++it )
//        kDebug(1212) << "CL2:" << *it;
//    mains = mainClients();
//    for ( ClientList::ConstIterator it = mains.begin();
//         it != mains.end();
//         ++it )
//        kDebug(1212) << "MN2:" << *it;
    for (ClientList::ConstIterator it = transients_list.constBegin();
            it != transients_list.constEnd();
       ) {
        if ((*it)->transientFor() == this) {
            removeTransient(*it);
            it = transients_list.constBegin(); // restart, just in case something more has changed with the list
        } else
            ++it;
    }
//    kDebug(1212) << "CLEANGROUPING3:" << this;
//    for ( ClientList::ConstIterator it = group()->members().begin();
//         it != group()->members().end();
//         ++it )
//        kDebug(1212) << "CL3:" << *it;
//    mains = mainClients();
//    for ( ClientList::ConstIterator it = mains.begin();
//         it != mains.end();
//         ++it )
//        kDebug(1212) << "MN3:" << *it;
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
//    kDebug(1212) << "CLEANGROUPING4:" << this;
//    for ( ClientList::ConstIterator it = group_members.begin();
//         it != group_members.end();
//         ++it )
//        kDebug(1212) << "CL4:" << *it;
}

// Make sure that no group transient is considered transient
// for a window that is (directly or indirectly) transient for it
// (including another group transients).
// Non-group transients not causing loops are checked in verifyTransientFor().
void Client::checkGroupTransients()
{
    TRANSIENCY_CHECK(this);
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
            for (Client* cl = (*it2)->transientFor();
                    cl != NULL;
                    cl = cl->transientFor()) {
                if (cl == *it1) {
                    // don't use removeTransient(), that would modify *it2 too
                    (*it2)->transients_list.removeAll(*it1);
                    continue;
                }
            }
            // if *it1 and *it2 are both group transients, and are transient for each other,
            // make only *it2 transient for *it1 (i.e. subwindow), as *it2 came later,
            // and should be therefore on top of *it1
            // TODO This could possibly be optimized, it also requires hasTransient() to check for loops.
            if ((*it2)->groupTransient() && (*it1)->hasTransient(*it2, true) && (*it2)->hasTransient(*it1, true))
                (*it2)->transients_list.removeAll(*it1);
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
                        (*it2)->transients_list.removeAll(*it1);
                    if ((*it3)->hasTransient(*it2, true))
                        (*it3)->transients_list.removeAll(*it1);
                }
            }
        }
    }
}

/*!
  Check that the window is not transient for itself, and similar nonsense.
 */
Window Client::verifyTransientFor(Window new_transient_for, bool defined)
{
    Window new_property_value = new_transient_for;
    // make sure splashscreens are shown above all their app's windows, even though
    // they're in Normal layer
    if (isSplash() && new_transient_for == None)
        new_transient_for = rootWindow();
    if (new_transient_for == None) {
        if (defined)   // sometimes WM_TRANSIENT_FOR is set to None, instead of root window
            new_property_value = new_transient_for = rootWindow();
        else
            return None;
    }
    if (new_transient_for == window()) { // pointing to self
        // also fix the property itself
        kWarning(1216) << "Client " << this << " has WM_TRANSIENT_FOR poiting to itself." ;
        new_property_value = new_transient_for = rootWindow();
    }
//  The transient_for window may be embedded in another application,
//  so kwin cannot see it. Try to find the managed client for the
//  window and fix the transient_for property if possible.
    WId before_search = new_transient_for;
    while (new_transient_for != None
            && new_transient_for != rootWindow()
            && !workspace()->findClient(WindowMatchPredicate(new_transient_for))) {
        Window root_return, parent_return;
        Window* wins = NULL;
        unsigned int nwins;
        int r = XQueryTree(display(), new_transient_for, &root_return, &parent_return, &wins, &nwins);
        if (wins)
            XFree((void *) wins);
        if (r == 0)
            break;
        new_transient_for = parent_return;
    }
    if (Client* new_transient_for_client = workspace()->findClient(WindowMatchPredicate(new_transient_for))) {
        if (new_transient_for != before_search) {
            kDebug(1212) << "Client " << this << " has WM_TRANSIENT_FOR poiting to non-toplevel window "
                         << before_search << ", child of " << new_transient_for_client << ", adjusting." << endl;
            new_property_value = new_transient_for; // also fix the property
        }
    } else
        new_transient_for = before_search; // nice try
// loop detection
// group transients cannot cause loops, because they're considered transient only for non-transient
// windows in the group
    int count = 20;
    Window loop_pos = new_transient_for;
    while (loop_pos != None && loop_pos != rootWindow()) {
        Client* pos = workspace()->findClient(WindowMatchPredicate(loop_pos));
        if (pos == NULL)
            break;
        loop_pos = pos->transient_for_id;
        if (--count == 0 || pos == this) {
            kWarning(1216) << "Client " << this << " caused WM_TRANSIENT_FOR loop." ;
            new_transient_for = rootWindow();
        }
    }
    if (new_transient_for != rootWindow()
            && workspace()->findClient(WindowMatchPredicate(new_transient_for)) == NULL) {
        // it's transient for a specific window, but that window is not mapped
        new_transient_for = rootWindow();
    }
    if (new_property_value != original_transient_for_id)
        XSetTransientForHint(display(), window(), new_property_value);
    return new_transient_for;
}

void Client::addTransient(Client* cl)
{
    TRANSIENCY_CHECK(this);
    assert(!transients_list.contains(cl));
//    assert( !cl->hasTransient( this, true )); will be fixed in checkGroupTransients()
    assert(cl != this);
    transients_list.append(cl);
    if (workspace()->mostRecentlyActivatedClient() == this && cl->isModal())
        check_active_modal = true;
//    kDebug(1212) << "ADDTRANS:" << this << ":" << cl;
//    kDebug(1212) << kBacktrace();
//    for ( ClientList::ConstIterator it = transients_list.begin();
//         it != transients_list.end();
//         ++it )
//        kDebug(1212) << "AT:" << (*it);
}

void Client::removeTransient(Client* cl)
{
    TRANSIENCY_CHECK(this);
//    kDebug(1212) << "REMOVETRANS:" << this << ":" << cl;
//    kDebug(1212) << kBacktrace();
    transients_list.removeAll(cl);
    // cl is transient for this, but this is going away
    // make cl group transient
    if (cl->transientFor() == this) {
        cl->transient_for_id = None;
        cl->transient_for = NULL; // SELI
// SELI       cl->setTransient( rootWindow());
        cl->setTransient(None);
    }
}

// A new window has been mapped. Check if it's not a mainwindow for this already existing window.
void Client::checkTransient(Window w)
{
    TRANSIENCY_CHECK(this);
    if (original_transient_for_id != w)
        return;
    w = verifyTransientFor(w, true);
    setTransient(w);
}

// returns true if cl is the transient_for window for this client,
// or recursively the transient_for window
bool Client::hasTransient(const Client* cl, bool indirect) const
{
    // checkGroupTransients() uses this to break loops, so hasTransient() must detect them
    ConstClientList set;
    return hasTransientInternal(cl, indirect, set);
}

bool Client::hasTransientInternal(const Client* cl, bool indirect, ConstClientList& set) const
{
    if (cl->transientFor() != NULL) {
        if (cl->transientFor() == this)
            return true;
        if (!indirect)
            return false;
        if (set.contains(cl))
            return false;
        set.append(cl);
        return hasTransientInternal(cl->transientFor(), indirect, set);
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
    for (ClientList::ConstIterator it = transients().constBegin();
            it != transients().constEnd();
            ++it)
        if ((*it)->hasTransientInternal(cl, indirect, set))
            return true;
    return false;
}

ClientList Client::mainClients() const
{
    if (!isTransient())
        return ClientList();
    if (transientFor() != NULL)
        return ClientList() << const_cast< Client* >(transientFor());
    ClientList result;
    for (ClientList::ConstIterator it = group()->members().constBegin();
            it != group()->members().constEnd();
            ++it)
        if ((*it)->hasTransient(this, false))
            result.append(*it);
    return result;
}

ClientList Client::allMainClients() const
{
    ClientList result = mainClients();
    foreach (const Client * cl, result)
    result += cl->allMainClients();
    return result;
}

Client* Client::findModal(bool allow_itself)
{
    for (ClientList::ConstIterator it = transients().constBegin();
            it != transients().constEnd();
            ++it)
        if (Client* ret = (*it)->findModal(true))
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
    TRANSIENCY_CHECK(this);
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
    } else if (window_group != None) {
        Group* new_group = workspace()->findGroup(window_group);
        if (transientFor() != NULL && transientFor()->group() != new_group) {
            // move the window to the right group (e.g. a dialog provided
            // by different app, but transient for this one, so make it part of that group)
            new_group = transientFor()->group();
        }
        if (new_group == NULL)   // doesn't exist yet
            new_group = new Group(window_group, workspace());
        if (new_group != in_group) {
            if (in_group != NULL)
                in_group->removeMember(this);
            in_group = new_group;
            in_group->addMember(this);
        }
    } else {
        if (transientFor() != NULL) {
            // doesn't have window group set, but is transient for something
            // so make it part of that group
            Group* new_group = transientFor()->group();
            if (new_group != in_group) {
                if (in_group != NULL)
                    in_group->removeMember(this);
                in_group = transientFor()->group();
                in_group->addMember(this);
            }
        } else if (groupTransient()) {
            // group transient which actually doesn't have a group :(
            // try creating group with other windows with the same client leader
            Group* new_group = workspace()->findClientLeaderGroup(this);
            if (new_group == NULL)
                new_group = new Group(None, workspace());
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
                new_group = new Group(None, workspace());
            if (in_group != new_group) {
                in_group = new_group;
                in_group->addMember(this);
            }
        }
    }
    if (in_group != old_group || force) {
        for (ClientList::Iterator it = transients_list.begin();
                it != transients_list.end();
           ) {
            // group transients in the old group are no longer transient for it
            if ((*it)->groupTransient() && (*it)->group() != group())
                it = transients_list.erase(it);
            else
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
    if (window_group)
        return;
    checkGroup(gr);   // change group
}

bool Client::check_active_modal = false;

void Client::checkActiveModal()
{
    // if the active window got new modal transient, activate it.
    // cannot be done in AddTransient(), because there may temporarily
    // exist loops, breaking findModal
    Client* check_modal = workspace()->mostRecentlyActivatedClient();
    if (check_modal != NULL && check_modal->check_active_modal) {
        Client* new_modal = check_modal->findModal();
        if (new_modal != NULL && new_modal != check_modal) {
            if (!new_modal->isManaged())
                return; // postpone check until end of manage()
            workspace()->activateClient(new_modal);
        }
        check_modal->check_active_modal = false;
    }
}

} // namespace

/*******************************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2011/2012 The KWin team <kwin@kde.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#include "tabgroup.h"

#include "client.h"
#include "effects.h"

namespace KWin
{

TabGroup::TabGroup(Client *c)
    : m_clients()
    , m_current(c)
    , m_minSize(c->minSize())
    , m_maxSize(c->maxSize())
    , m_stateUpdatesBlocked(0)
    , m_pendingUpdates(TabGroup::None)
{
    QIcon icon(c->icon());
    icon.addPixmap(c->miniIcon());
    m_clients << c;
    c->setTabGroup(this);
    c->setClientShown(true);
}

TabGroup::~TabGroup()
{
}


void TabGroup::activateNext()
{
    int index = m_clients.indexOf(m_current);
    setCurrent(m_clients.at((index < m_clients.count() - 1) ? index + 1 : 0));
}

void TabGroup::activatePrev()
{
    int index = m_clients.indexOf(m_current);
    setCurrent(m_clients.at((index > 0) ? index - 1 : m_clients.count() - 1));
}

bool TabGroup::add(Client* c, Client *other, bool after, bool becomeVisible)
{
    Q_ASSERT(!c->tabGroup());

    if (!c->workspace()->decorationSupportsTabbing() || contains(c) || !contains(other))
        return false;

    // Tabbed windows MUST have a decoration
    c->setNoBorder(false);
    if (c->noBorder())
        return false;

    // If it's not possible to have the same states then ungroup them, TODO: Check all states
    // We do this here as the ungroup code in updateStates() cannot be called until add() completes

    bool cannotTab = false;
    ShadeMode oldShadeMode = c->shadeMode();
    QRect oldGeom = c->geometry();
    int oldDesktop = c->desktop();

    c->setShade(m_current->shadeMode());
    cannotTab = c->shadeMode() != m_current->shadeMode();
    if (!cannotTab) {
        c->setDesktop(m_current->desktop());
        cannotTab = c->desktop() != m_current->desktop();
    }
    if (!cannotTab) {
        c->setGeometry(m_current->geometry());
        cannotTab = c->geometry() != m_current->geometry();
    }

    if (cannotTab) {
        c->setShade(oldShadeMode);
        c->setDesktop(oldDesktop);
        c->setGeometry(oldGeom);
        // trigger decoration repaint on the group to make sure that hover animations are properly reset.
        m_current->triggerDecorationRepaint();
        return false; // cannot tab
    }

    // Actually add to new group ----------------------------------------

    // Notify effects of merge
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->slotTabAdded(c->effectWindow(), other->effectWindow());

    // next: aling the client states BEFORE adding it to the group
    // otherwise the caused indirect state changes would be taken as the dominating ones and break
    // the main client
    // example: QuickTiling is aligned to None, this restores the former QuickTiled size and alignes
    // all other windows in the group - including the actual main client! - to this size and thus
    // breaks the actually required alignment to the main windows geometry (because that now has the
    // restored geometry of the formerly Q'tiled window) - bug #303937
    updateStates(m_current, All, c);

    int index = other ? m_clients.indexOf(other) : m_clients.size();
    index += after;
    if (index > m_clients.size())
        index = m_clients.size();

    m_clients.insert(index, c);

    c->setTabGroup(this);   // Let the client know which group it belongs to

    updateMinMaxSize();

    if (!becomeVisible)
        c->setClientShown(false);
    else {
        c->setClientShown(true);
        if (!effects || c->readyForPainting()) {
            setCurrent(c);
            if (options->focusPolicyIsReasonable())
                m_current->workspace()->requestFocus( c );
        }
        else {
            if (options->focusPolicyIsReasonable())
                m_current->workspace()->requestFocus( m_current );
            m_current = c; // setCurrent will be called by Toplevel::setReadyForPainting()
        }
    }

    m_current->triggerDecorationRepaint();
    return true;
}

bool TabGroup::remove(Client* c, const QRect& newGeom)
{
    if (!c)
        return false;

    int index = m_clients.indexOf(c);
    if (index < 0)
        return false;

    c->setTabGroup(NULL);

    m_clients.removeAt(index);
    updateMinMaxSize();

    if (m_clients.count() == 1) { // split
        remove(m_clients.at(0), m_clients.at(0)->geometry());
    }
    if (m_clients.isEmpty()) { // remaining singleton "tab"
        c->setClientShown(true);
        return true; // group is gonna be deleted after this anyway
    }

    if (c == m_current) {
        m_current = index < m_clients.count() ? m_clients.at(index) : m_clients.last();
        m_current->setClientShown(true);

        if (effects) // "c -> m_current" because c was m_current
            static_cast<EffectsHandlerImpl*>(effects)->slotCurrentTabAboutToChange(c->effectWindow(), m_current->effectWindow());
    }

    if (c->quickTileMode() != QuickTileNone)
        c->setQuickTileMode(QuickTileNone); // if we leave a quicktiled group, assume that the user wants to untile
    else if (newGeom.isValid()) {
        c->maximize(Client::MaximizeRestore); // explicitly calling for a geometry -> unmaximize - in doubt
        c->setGeometry(newGeom);
        c->checkWorkspacePosition(); // oxygen has now twice kicked me a window out of the screen - better be safe then sorry
    }

    // Notify effects of removal
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->slotTabRemoved(c->effectWindow(), m_current->effectWindow());

    m_current->triggerDecorationRepaint();
    return true;
}

void TabGroup::closeAll()
{
    // NOTICE - in theory it's OK to use the list because closing sends an event to the client and
    // due to the async X11 nature, the client would be released and thus removed from m_clients
    // after this function exits.
    // However later Wayland support or similar might not share this bahaviour - and we really had
    // enough trouble with a polluted client list around the tabbing code ....
    ClientList list(m_clients);
    for (ClientList::const_iterator i = list.constBegin(), end = list.constEnd(); i != end; ++i)
        if (*i != m_current)
            (*i)->closeWindow();

    m_current->closeWindow();
}

void TabGroup::move(Client *c, Client *other, bool after)
{
    if (c == other)
        return;

    int from = m_clients.indexOf(c);
    if (from < 0)
        return;

    int to = other ? m_clients.indexOf(other) : m_clients.size() - 1;
    if (to < 0)
        return;
    to += after;
    if (to >= m_clients.size())
        to = m_clients.size() - 1;

    if (from == to)
        return;

    m_clients.move(from, to);
    m_current->triggerDecorationRepaint();
}

bool TabGroup::isActive() const
{
    return contains(Workspace::self()->activeClient());
}

void TabGroup::setCurrent(Client* c, bool force)
{
    if ((c == m_current && !force) || !contains(c))
        return;

    // Notify effects of switch
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->slotCurrentTabAboutToChange(m_current->effectWindow(), c->effectWindow());

    m_current = c;
    c->setClientShown(true); // reduce flicker?
    for (ClientList::const_iterator i = m_clients.constBegin(), end = m_clients.constEnd(); i != end; ++i)
        (*i)->setClientShown((*i) == m_current);
}

void TabGroup::sync(const char *property, Client *c)
{
    if (c->metaObject()->indexOfProperty(property) > -1) {
        qWarning("caught attempt to sync non dynamic property: %s", property);
        return;
    }
    QVariant v = c->property(property);
    for (ClientList::iterator i = m_clients.begin(), end = m_clients.end(); i != end; ++i) {
        if (*i != m_current)
            (*i)->setProperty(property, v);
    }
}

void TabGroup::updateMinMaxSize()
{
    // Determine entire group's minimum and maximum sizes
    // TODO this used to be signalled out but i didn't find a receiver - or got an idea what this would be good for
    // find purpose & solution or kick it
//     QSize oldMin = m_minSize;
//     QSize oldMax = m_maxSize;
    m_minSize = QSize(0, 0);
    m_maxSize = QSize(INT_MAX, INT_MAX);

    for (ClientList::const_iterator i = m_clients.constBegin(); i != m_clients.constEnd(); ++i) {
        m_minSize = m_minSize.expandedTo((*i)->minSize());
        m_maxSize = m_maxSize.boundedTo((*i)->maxSize());
    }

    // TODO: this actually resolves a conflict that should be caught when adding?
    m_maxSize = m_maxSize.expandedTo(m_minSize);

    // calculate this _once_ to get a common size.
    // TODO this leaves another unresolved conflict about the base increment (luckily not used too often)
    const QSize size = m_current->clientSize().expandedTo(m_minSize).boundedTo(m_maxSize);
    if (size != m_current->clientSize()) {
        const QRect r(m_current->pos(), m_current->sizeForClientSize(size));
        for (ClientList::const_iterator i = m_clients.constBegin(), end = m_clients.constEnd(); i != end; ++i)
            (*i)->setGeometry(r);
    }
}


void TabGroup::blockStateUpdates(bool more) {
    more ? ++m_stateUpdatesBlocked : --m_stateUpdatesBlocked;
    if (m_stateUpdatesBlocked < 0) {
        m_stateUpdatesBlocked = 0;
        qWarning("TabGroup: Something is messed up with TabGroup::blockStateUpdates() invocation\nReleased more than blocked!");
    }
}

void TabGroup::updateStates(Client* main, States states, Client* only)
{
    if (main == only)
        return; // there's no need to only align "us" to "us"
    if (m_stateUpdatesBlocked > 0) {
        m_pendingUpdates |= states;
        return;
    }

    states |= m_pendingUpdates;
    m_pendingUpdates = TabGroup::None;

    ClientList toBeRemoved, onlyDummy;
    ClientList *list = &m_clients;
    if (only) {
        onlyDummy << only;
        list = &onlyDummy;
    }

    for (ClientList::const_iterator i = list->constBegin(), end = list->constEnd(); i != end; ++i) {
        Client *c = (*i);
        if (c != main) {
            if ((states & Minimized) && c->isMinimized() != main->isMinimized()) {
                if (main->isMinimized())
                    c->minimize(true);
                else
                    c->unminimize(true);
            }

            // the order QuickTile -> Maximized -> Geometry is somewhat important because one will change the other
            // don't change w/o good reason and care
            if ((states & QuickTile) && c->quickTileMode() != main->quickTileMode())
                c->setQuickTileMode(main->quickTileMode());
            if ((states & Maximized) && c->maximizeMode() != main->maximizeMode())
                c->maximize(main->maximizeMode());
            // the order Shaded -> Geometry is somewhat important because one will change the other
            if ((states & Shaded) && c->isShade() != main->isShade())
                c->setShade(main->isShade() ? ShadeNormal : ShadeNone);
            if ((states & Geometry) && c->geometry() != main->geometry())
                c->setGeometry(main->geometry());
            if (states & Desktop) {
                if (c->isOnAllDesktops() != main->isOnAllDesktops())
                    c->setOnAllDesktops(main->isOnAllDesktops());
                if (c->desktop() != main->desktop())
                    c->setDesktop(main->desktop());
            }
            if ((states & Activity) && c->activities() != main->activities()) {
                c->setOnActivities(main->activities());
            }
            if (states & Layer) {
                if (c->keepAbove() != main->keepAbove())
                    c->setKeepAbove(main->keepAbove());
                if (c->keepBelow() != main->keepBelow())
                    c->setKeepBelow(main->keepBelow());
            }

            // If it's not possible to have the same states then ungroup them, TODO: Check all states
            if (((states & Geometry) && c->geometry() != main->geometry()) ||
                ((states & Desktop) && c->desktop() != main->desktop()))
                toBeRemoved << c;
        }
    }

    for (ClientList::const_iterator i = toBeRemoved.constBegin(), end = toBeRemoved.constEnd(); i != end; ++i)
        remove(*i);
}

}

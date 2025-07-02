/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// SELI zmenit doc

/*

 This file contains things relevant to stacking order and layers.

 Design:

 Normal unconstrained stacking order, as requested by the user (by clicking
 on windows to raise them, etc.), is in Workspace::unconstrained_stacking_order.
 That list shouldn't be used at all, except for building
 Workspace::stacking_order. The building is done
 in Workspace::constrainedStackingOrder(). Only Workspace::stackingOrder() should
 be used to get the stacking order, because it also checks the stacking order
 is up to date.
 All clients are also stored in Workspace::clients (except for isDesktop() clients,
 as those are very special, and are stored in Workspace::desktops), in the order
 the clients were created.

 Every window has one layer assigned in which it is. There are 7 layers,
 from bottom : DesktopLayer, BelowLayer, NormalLayer, DockLayer, AboveLayer, NotificationLayer,
 ActiveLayer, CriticalNotificationLayer, and OnScreenDisplayLayer (see also NETWM sect.7.10.).
 The layer a window is in depends on the window type, and on other things like whether the window
 is active. We extend the layers provided in NETWM by the NotificationLayer, OnScreenDisplayLayer,
 and CriticalNotificationLayer.
 The NoficationLayer contains notification windows which are kept above all windows except the active
 fullscreen window. The CriticalNotificationLayer contains notification windows which are important
 enough to keep them even above fullscreen windows. The OnScreenDisplayLayer is used for eg. volume
 and brightness change feedback and is kept above all windows since it provides immediate response
 to a user action.

 NET::Splash clients belong to the Normal layer. NET::TopMenu clients
 belong to Dock layer. Clients that are both NET::Dock and NET::KeepBelow
 are in the Normal layer in order to keep the 'allow window to cover
 the panel' Kicker setting to work as intended (this may look like a slight
 spec violation, but a) I have no better idea, b) the spec allows adjusting
 the stacking order if the WM thinks it's a good idea . We put all
 NET::KeepAbove above all Docks too, even though the spec suggests putting
 them in the same layer.

 Most transients are in the same layer as their mainwindow,
 see Workspace::constrainedStackingOrder(), they may also be in higher layers, but
 they should never be below their mainwindow.

 Currently the things that affect client in which layer a client
 belongs: KeepAbove/Keep Below flags, window type, fullscreen
 state and whether the client is active, mainclient (transiency).

 Make sure updateStackingOrder() is called in order to make
 Workspace::stackingOrder() up to date and propagated to the world.
 Using Workspace::blockStackingUpdates() (or the StackingUpdatesBlocker
 helper class) it's possible to temporarily disable updates
 and the stacking order will be updated once after it's allowed again.

*/

#include "compositor.h"
#include "effect/effecthandler.h"
#include "focuschain.h"
#include "internalwindow.h"
#include "rules.h"
#include "screenedge.h"
#include "tabbox/tabbox.h"
#include "utils/common.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"
#if KWIN_BUILD_X11
#include "group.h"
#include "netinfo.h"
#include "x11window.h"
#endif

#include <array>

#include <QDebug>

namespace KWin
{

//*******************************
// Workspace
//*******************************

void Workspace::updateStackingOrder(bool propagate_new_windows)
{
    if (m_blockStackingUpdates > 0) {
        if (propagate_new_windows) {
            m_blockedPropagatingNewWindows = true;
        }
        return;
    }
    QList<Window *> new_stacking_order = constrainedStackingOrder();
    bool changed = (force_restacking || new_stacking_order != stacking_order);
    force_restacking = false;
    stacking_order = new_stacking_order;
    if (changed || propagate_new_windows) {
#if KWIN_BUILD_X11
        propagateWindows(propagate_new_windows);
#endif

        for (int i = 0; i < stacking_order.size(); ++i) {
            stacking_order[i]->setStackingOrder(i);
        }

        Q_EMIT stackingOrderChanged();

        if (m_activeWindow) {
            m_activeWindow->updateMouseGrab();
        }
    }
}

#if KWIN_BUILD_X11
/**
 * Some fullscreen effects have to raise the screenedge on top of an input window, thus all windows
 * this function puts them back where they belong for regular use and is some cheap variant of
 * the regular propagateWindows function in that it completely ignores managed windows and everything
 * else and also does not update the NETWM property.
 * Called from Effects::destroyInputWindow so far.
 */
void Workspace::stackScreenEdgesUnderOverrideRedirect()
{
    if (!rootInfo()) {
        return;
    }
    Xcb::restackWindows(QList<xcb_window_t>() << rootInfo()->supportWindow() << workspace()->screenEdges()->windows());
}

/**
 * Propagates the managed windows to the world.
 * Called ONLY from updateStackingOrder().
 */
void Workspace::propagateWindows(bool propagate_new_windows)
{
    if (!rootInfo()) {
        return;
    }
    // restack the windows according to the stacking order
    // supportWindow > electric borders > windows > hidden windows
    QList<xcb_window_t> newWindowStack;

    // Stack all windows under the support window. The support window is
    // not used for anything (besides the NETWM property), and it's not shown,
    // but it was lowered after kwin startup. Stacking all windows below
    // it ensures that no window will be ever shown above override-redirect
    // windows (e.g. popups).
    newWindowStack << rootInfo()->supportWindow();

    newWindowStack << workspace()->screenEdges()->windows();

    newWindowStack << manual_overlays;

    newWindowStack.reserve(newWindowStack.size() + 2 * stacking_order.size()); // *2 for inputWindow

    for (int i = stacking_order.size() - 1; i >= 0; --i) {
        X11Window *window = qobject_cast<X11Window *>(stacking_order.at(i));
        if (!window || window->isDeleted() || window->isUnmanaged() || window->hiddenPreview()) {
            continue;
        }

        if (window->inputId()) {
            // Stack the input window above the frame
            newWindowStack << window->inputId();
        }

        newWindowStack << window->frameId();
    }

    // when having hidden previews, stack hidden windows below everything else
    // (as far as pure X stacking order is concerned), in order to avoid having
    // these windows that should be unmapped to interfere with other windows
    for (int i = stacking_order.size() - 1; i >= 0; --i) {
        X11Window *window = qobject_cast<X11Window *>(stacking_order.at(i));
        if (!window || window->isDeleted() || window->isUnmanaged() || !window->hiddenPreview()) {
            continue;
        }
        newWindowStack << window->frameId();
    }
    // TODO isn't it too inefficient to restack always all windows?
    // TODO don't restack not visible windows?
    Q_ASSERT(newWindowStack.at(0) == rootInfo()->supportWindow());
    Xcb::restackWindows(newWindowStack);

    QList<xcb_window_t> cl;
    if (propagate_new_windows) {
        cl.reserve(manual_overlays.size() + m_windows.size());
        for (const auto win : std::as_const(manual_overlays)) {
            cl.push_back(win);
        }
        for (Window *window : std::as_const(m_windows)) {
            X11Window *x11Window = qobject_cast<X11Window *>(window);
            if (x11Window && !x11Window->isUnmanaged()) {
                cl.push_back(x11Window->window());
            }
        }
        rootInfo()->setClientList(cl.constData(), cl.size());
    }

    cl.clear();
    for (auto it = stacking_order.constBegin(); it != stacking_order.constEnd(); ++it) {
        X11Window *window = qobject_cast<X11Window *>(*it);
        if (window && !window->isDeleted() && !window->isUnmanaged()) {
            cl.push_back(window->window());
        }
    }
    for (const auto win : std::as_const(manual_overlays)) {
        cl.push_back(win);
    }
    rootInfo()->setClientListStacking(cl.constData(), cl.size());
}
#endif

/**
 * Returns topmost visible window. Windows on the dock, the desktop
 * or of any other special kind are excluded. Also if the window
 * doesn't accept focus it's excluded.
 */
// TODO misleading name for this method, too many slightly different ways to use it
Window *Workspace::topWindowOnDesktop(VirtualDesktop *desktop, Output *output, bool unconstrained, bool only_normal) const
{
    // TODO    Q_ASSERT( block_stacking_updates == 0 );
    QList<Window *> list;
    if (!unconstrained) {
        list = stacking_order;
    } else {
        list = unconstrained_stacking_order;
    }
    for (int i = list.size() - 1; i >= 0; --i) {
        auto window = list.at(i);
        if (!window->isClient() || window->isDeleted()) {
            continue;
        }
        if (window->isOnDesktop(desktop) && window->isShown() && window->isOnCurrentActivity() && !window->isShade()) {
            if (output && window->output() != output) {
                continue;
            }
            if (!only_normal) {
                return window;
            }
            if (window->wantsTabFocus() && !window->isSpecialWindow()) {
                return window;
            }
        }
    }
    return nullptr;
}

Window *Workspace::findDesktop(VirtualDesktop *desktop, Output *output) const
{
    // TODO    Q_ASSERT( block_stacking_updates == 0 );
    for (int i = stacking_order.size() - 1; i >= 0; i--) {
        auto window = stacking_order.at(i);
        if (window->isDeleted()) {
            continue;
        }
        if (window->isClient() && window->isOnDesktop(desktop) && window->isOnOutput(output) && window->isDesktop() && window->isShown()) {
            return window;
        }
    }
    return nullptr;
}

#if KWIN_BUILD_X11
static Layer layerForWindow(const X11Window *window)
{
    Layer layer = window->layer();

    // Desktop windows cannot be promoted to upper layers.
    if (layer == DesktopLayer) {
        return layer;
    }

    if (const Group *group = window->group()) {
        const auto members = group->members();
        for (const X11Window *member : members) {
            if (member == window) {
                continue;
            } else if (member->output() != window->output()) {
                continue;
            }
            if (member->layer() == ActiveLayer) {
                return ActiveLayer;
            }
        }
    }

    return layer;
}
#endif

static Layer computeLayer(const Window *window)
{
#if KWIN_BUILD_X11
    if (auto x11Window = qobject_cast<const X11Window *>(window)) {
        return layerForWindow(x11Window);
    }
#endif
    return window->layer();
}

bool Workspace::areConstrained(const Window *below, const Window *above) const
{
    for (const auto constraint : std::as_const(m_constraints)) {
        if (constraint->below == below) {
            if (constraint->above == above) {
                return true;
            } else {
                for (const auto child : std::as_const(constraint->children)) {
                    if (areConstrained(child->below, above)) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void Workspace::raiseOrLowerWindow(Window *window)
{
    if (!window->isOnCurrentDesktop()) {
        return;
    }

    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();
    Output *output = options->isSeparateScreenFocus() ? window->output() : nullptr;
    Layer layer = computeLayer(window);

    bool topmost = false;
    for (auto it = unconstrained_stacking_order.crbegin(); it != unconstrained_stacking_order.crend(); ++it) {
        if (layer != computeLayer(*it) || !(*it)->isClient() || (*it)->isDeleted()) {
            continue;
        }
        if ((*it)->isOnDesktop(desktop) && (*it)->isShown() && (*it)->isOnCurrentActivity() && !(*it)->isShade()) {
            if (output && (*it)->output() != output) {
                continue;
            }
            if ((*it)->wantsTabFocus() && !(*it)->isSpecialWindow()) {
                if (*it == window) {
                    topmost = true;
                    break;
                } else if (areConstrained(window, *it)) {
                    continue; // window `it' must be above us anyway, ignore it
                } else {
                    break;
                }
            }
        }
    }

    if (topmost) {
        lowerWindow(window);
    } else {
        raiseWindow(window);
    }
}

void Workspace::lowerWindow(Window *window, bool nogroup)
{
    if (window->isDeleted()) {
        qCWarning(KWIN_CORE) << "Workspace::lowerWindow: closed window" << window << "cannot be restacked";
        return;
    }

    window->cancelAutoRaise();

    StackingUpdatesBlocker blocker(this);

    if (nogroup || (!window->isTransient() && window->transients().isEmpty())) {
        unconstrained_stacking_order.removeAll(window);
        unconstrained_stacking_order.prepend(window);
    } else {
        auto mainWindows = window->allMainWindows();
        Window *parent;
        if (mainWindows.isEmpty()) {
            parent = window;
        } else {
            parent = ensureStackingOrder(mainWindows).front();
        }
        QList<Window *> windows{parent};
        for (int i = 0; i < windows.size(); ++i) {
            if (!windows[i]->transients().isEmpty()) {
                windows << windows[i]->transients();
            }
        }
        windows = ensureStackingOrder(windows);
        for (int i = windows.size() - 1; i >= 0; --i) {
            lowerWindow(windows[i], true);
        }
    }
}

void Workspace::lowerWindowWithinApplication(Window *window)
{
    if (window->isDeleted()) {
        qCWarning(KWIN_CORE) << "Workspace::lowerWindowWithinApplication: closed window" << window << "cannot be restacked";
        return;
    }

    window->cancelAutoRaise();

    StackingUpdatesBlocker blocker(this);

    unconstrained_stacking_order.removeAll(window);
    bool lowered = false;
    // first try to put it below the bottom-most window of the application
    for (auto it = unconstrained_stacking_order.begin(); it != unconstrained_stacking_order.end(); ++it) {
        auto other = *it;
        if (!other->isClient() || other->isDeleted()) {
            continue;
        }
        if (Window::belongToSameApplication(other, window)) {
            unconstrained_stacking_order.insert(it, window);
            lowered = true;
            break;
        }
    }
    if (!lowered) {
        unconstrained_stacking_order.prepend(window);
    }
    // ignore mainwindows
}

void Workspace::raiseWindow(Window *window, bool nogroup)
{
    if (window->isDeleted()) {
        qCWarning(KWIN_CORE) << "Workspace::raiseWindow: closed window" << window << "cannot be restacked";
        return;
    }

    window->cancelAutoRaise();

    StackingUpdatesBlocker blocker(this);

    if (!nogroup && window->isTransient()) {
        QList<Window *> transients;
        Window *transient_parent = window;
        while ((transient_parent = transient_parent->transientFor())) {
            transients.prepend(transient_parent);
        }
        for (const auto &transient_parent : std::as_const(transients)) {
            raiseWindow(transient_parent, true);
        }
    }

    unconstrained_stacking_order.removeAll(window);
    unconstrained_stacking_order.append(window);
}

void Workspace::raiseWindowWithinApplication(Window *window)
{
    if (window->isDeleted()) {
        qCWarning(KWIN_CORE) << "Workspace::raiseWindowWithinApplication: closed window" << window << "cannot be restacked";
        return;
    }

    window->cancelAutoRaise();

    StackingUpdatesBlocker blocker(this);
    // ignore mainwindows

    // first try to put it above the top-most window of the application
    for (int i = unconstrained_stacking_order.size() - 1; i > -1; --i) {
        auto other = unconstrained_stacking_order.at(i);
        if (!other->isClient() || other->isDeleted()) {
            continue;
        }
        if (other == window) { // don't lower it just because it asked to be raised
            return;
        }
        if (Window::belongToSameApplication(other, window)) {
            unconstrained_stacking_order.removeAll(window);
            unconstrained_stacking_order.insert(unconstrained_stacking_order.indexOf(other) + 1, window); // insert after the found one
            break;
        }
    }
}

#if KWIN_BUILD_X11
void Workspace::raiseWindowRequest(Window *window, NET::RequestSource src, xcb_timestamp_t timestamp)
{
    if (src == NET::FromTool || allowFullClientRaising(window, timestamp)) {
        raiseWindow(window);
    } else {
        raiseWindowWithinApplication(window);
        window->demandAttention();
    }
}

void Workspace::lowerWindowRequest(X11Window *window, NET::RequestSource src, xcb_timestamp_t /*timestamp*/)
{
    // If the window has support for all this focus stealing prevention stuff,
    // do only lowering within the application, as that's the more logical
    // variant of lowering when application requests it.
    // No demanding of attention here of course.
    if (src == NET::FromTool || !window->hasUserTimeSupport()) {
        lowerWindow(window);
    } else {
        lowerWindowWithinApplication(window);
    }
}
#endif

void Workspace::stackBelow(Window *window, Window *reference)
{
    if (window->isDeleted()) {
        qCWarning(KWIN_CORE) << "Workspace::stackBelow: closed window" << window << "cannot be restacked";
        return;
    }

    Q_ASSERT(unconstrained_stacking_order.contains(reference));
    if (reference == window) {
        return;
    }

    unconstrained_stacking_order.removeAll(window);
    unconstrained_stacking_order.insert(unconstrained_stacking_order.indexOf(reference), window);

    m_focusChain->moveAfterWindow(window, reference);
    updateStackingOrder();
}

void Workspace::stackAbove(Window *window, Window *reference)
{
    if (window->isDeleted()) {
        qCWarning(KWIN_CORE) << "Workspace::stackAbove: closed window" << window << "cannot be restacked";
        return;
    }

    Q_ASSERT(unconstrained_stacking_order.contains(reference));
    if (reference == window) {
        return;
    }

    unconstrained_stacking_order.removeAll(window);
    unconstrained_stacking_order.insert(unconstrained_stacking_order.indexOf(reference) + 1, window);

    m_focusChain->moveBeforeWindow(window, reference);
    updateStackingOrder();
}

void Workspace::restackWindowUnderActive(Window *window)
{
    if (!m_activeWindow || m_activeWindow == window || m_activeWindow->layer() != window->layer()) {
        raiseWindow(window);
        return;
    }

    Window *reference = m_activeWindow;
    if (!Window::belongToSameApplication(reference, window)) {
        // put in the stacking order below _all_ windows belonging to the active application
        for (int i = 0; i < unconstrained_stacking_order.size(); ++i) {
            auto other = unconstrained_stacking_order.at(i);
            if (other->isClient() && other->layer() == window->layer() && Window::belongToSameApplication(reference, other)) {
                reference = other;
                break;
            }
        }
    }

    stackBelow(window, reference);
}

#if KWIN_BUILD_X11
void Workspace::restoreSessionStackingOrder(X11Window *window)
{
    if (window->sessionStackingOrder() < 0) {
        return;
    }
    StackingUpdatesBlocker blocker(this);
    unconstrained_stacking_order.removeAll(window);
    for (auto it = unconstrained_stacking_order.begin(); it != unconstrained_stacking_order.end(); ++it) {
        X11Window *current = qobject_cast<X11Window *>(*it);
        if (!current || current->isDeleted() || current->isUnmanaged()) {
            continue;
        }
        if (current->sessionStackingOrder() > window->sessionStackingOrder()) {
            unconstrained_stacking_order.insert(it, window);
            return;
        }
    }
    unconstrained_stacking_order.append(window);
}
#endif

/**
 * Returns a stacking order based upon \a list that fulfills certain contained.
 */
QList<Window *> Workspace::constrainedStackingOrder()
{
    // Sort the windows based on their layers while preserving their relative order in the
    // unconstrained stacking order.
    std::array<QList<Window *>, NumLayers> windows;
    for (Window *window : std::as_const(unconstrained_stacking_order)) {
        const Layer layer = computeLayer(window);
        windows[layer] << window;
    }

    QList<Window *> stacking;
    stacking.reserve(unconstrained_stacking_order.count());
    for (uint layer = FirstLayer; layer < NumLayers; ++layer) {
        stacking += windows[layer];
    }

    // Apply the stacking order constraints. First, we enqueue the root constraints, i.e.
    // the ones that are not affected by other constraints.
    QList<Constraint *> constraints;
    constraints.reserve(m_constraints.count());
    for (Constraint *constraint : std::as_const(m_constraints)) {
        if (constraint->parents.isEmpty()) {
            constraint->enqueued = true;
            constraints.append(constraint);
        } else {
            constraint->enqueued = false;
        }
    }

    // Preserve the relative order of transient siblings in the unconstrained stacking order.
    auto constraintComparator = [&stacking](Constraint *a, Constraint *b) {
        return stacking.indexOf(a->above) > stacking.indexOf(b->above);
    };
    std::sort(constraints.begin(), constraints.end(), constraintComparator);

    // Once we've enqueued all the root constraints, we traverse the constraints tree in
    // the reverse breadth-first search fashion. A constraint is applied only if its condition is
    // not met.
    while (!constraints.isEmpty()) {
        Constraint *constraint = constraints.takeFirst();

        const int belowIndex = stacking.indexOf(constraint->below);
        const int aboveIndex = stacking.indexOf(constraint->above);
        if (belowIndex == -1 || aboveIndex == -1) {
            continue;
        } else if (aboveIndex < belowIndex) {
            stacking.removeAt(aboveIndex);
            stacking.insert(belowIndex, constraint->above);
        }

        // Preserve the relative order of transient siblings in the unconstrained stacking order.
        QList<Constraint *> children = constraint->children;
        std::sort(children.begin(), children.end(), constraintComparator);

        for (Constraint *child : std::as_const(children)) {
            if (!child->enqueued) {
                child->enqueued = true;
                constraints.append(child);
            }
        }
    }

    return stacking;
}

void Workspace::blockStackingUpdates(bool block)
{
    if (block) {
        if (m_blockStackingUpdates == 0) {
            m_blockedPropagatingNewWindows = false;
        }
        ++m_blockStackingUpdates;
    } else // !block
        if (--m_blockStackingUpdates == 0) {
            updateStackingOrder(m_blockedPropagatingNewWindows);
            if (effects) {
                effects->checkInputWindowStacking();
            }
        }
}

namespace
{
template<class T>
QList<T *> ensureStackingOrderInList(const QList<Window *> &stackingOrder, const QList<T *> &list)
{
    static_assert(std::is_base_of<Window, T>::value,
                  "U must be derived from T");
    // TODO    Q_ASSERT( block_stacking_updates == 0 );
    if (list.count() < 2) {
        return list;
    }
    // TODO is this worth optimizing?
    QList<T *> result = list;
    for (auto it = stackingOrder.begin(); it != stackingOrder.end(); ++it) {
        T *window = qobject_cast<T *>(*it);
        if (!window) {
            continue;
        }
        if (result.removeAll(window) != 0) {
            result.append(window);
        }
    }
    return result;
}
}

#if KWIN_BUILD_X11
// Ensure list is in stacking order
QList<X11Window *> Workspace::ensureStackingOrder(const QList<X11Window *> &list) const
{
    return ensureStackingOrderInList(stacking_order, list);
}
#endif

QList<Window *> Workspace::ensureStackingOrder(const QList<Window *> &list) const
{
    return ensureStackingOrderInList(stacking_order, list);
}

QList<Window *> Workspace::unconstrainedStackingOrder() const
{
    return unconstrained_stacking_order;
}

#if KWIN_BUILD_X11
bool Workspace::updateXStackingOrder()
{
    // we use our stacking order for managed windows, but X's for override-redirect windows
    Xcb::Tree tree(kwinApp()->x11RootWindow());
    if (tree.isNull()) {
        return false;
    }
    xcb_window_t *windows = tree.children();

    const auto count = tree.data()->children_len;
    bool changed = false;
    for (unsigned int i = 0; i < count; ++i) {
        auto window = findUnmanaged(windows[i]);
        if (window) {
            unconstrained_stacking_order.removeAll(window);
            unconstrained_stacking_order.append(window);
            changed = true;
        }
    }
    return changed;
}
#endif

} // namespace

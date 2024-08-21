/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// KWin
#include "effect/globals.h"
// Qt
#include <QHash>
#include <QObject>

namespace KWin
{
// forward declarations
class Window;
class Output;
class VirtualDesktop;

/**
 * @brief Singleton class to handle the various focus chains.
 *
 * A focus chain is a list of Windows containing information on which Window should be activated.
 *
 * Internally this FocusChain holds multiple independent chains. There is one chain of most recently
 * used Windows which is primarily used by TabBox to build up the list of Windows for navigation.
 * The chains are organized as a normal QList of Windows with the most recently used Window being the
 * last item of the list, that is a LIFO like structure.
 *
 * In addition there is one chain for each virtual desktop which is used to determine which Window
 * should get activated when the user switches to another virtual desktop.
 *
 * Furthermore this class contains various helper methods for the two different kind of chains.
 */
class FocusChain : public QObject
{
    Q_OBJECT
public:
    enum Change {
        MakeFirst,
        MakeLast,
        Update,
        MakeFirstMinimized = MakeFirst
    };
    explicit FocusChain() = default;

    /**
     * @brief Updates the position of the @p window according to the requested @p change in the
     * focus chain.
     *
     * This method affects both the most recently used focus chain and the per virtual desktop focus
     * chain.
     *
     * In case the window does no longer want to get focus, it is removed from all chains. In case
     * the window is on all virtual desktops it is ensured that it is present in each of the virtual
     * desktops focus chain. In case it's on exactly one virtual desktop it is ensured that it is only
     * in the focus chain for that virtual desktop.
     *
     * Depending on @p change the Window is inserted at different positions in the focus chain. In case
     * of @c MakeFirst it is moved to the first position of the chain, in case of
     * @c MakeLast it is moved to the last position of the chain. In all other cases it
     * depends on whether the @p window is the currently active Window. If it is the active Window it
     * becomes the first Window in the chain, otherwise it is inserted at the second position that is
     * directly after the currently active Window.
     *
     * @param window The Window which should be moved inside the chains.
     * @param change Where to move the Window
     * @return void
     */
    void update(Window *window, Change change);
    /**
     * @brief Moves @p window behind the @p reference Window in all focus chains.
     *
     * @param window The Window to move in the chains
     * @param reference The Window behind which the @p window should be moved
     * @return void
     */
    void moveAfterWindow(Window *window, Window *reference);
    /**
     * @brief Moves @p window in front of the @p reference Window in all focus chains.
     *
     * @param window The Window to move in the chains
     * @param reference The Window in front of which the @p window should be moved
     * @return void
     */
    void moveBeforeWindow(Window *window, Window *reference);
    /**
     * @brief Finds the best Window to become the new active Window in the focus chain for the given
     * virtual @p desktop.
     *
     * In case that separate screen focus is used only Windows on the current screen are considered.
     * If no Window for activation is found @c null is returned.
     *
     * @param desktop The virtual desktop to look for a Window for activation
     * @return :X11Window *The Window which could be activated or @c null if there is none.
     */
    Window *getForActivation(VirtualDesktop *desktop) const;
    /**
     * @brief Finds the best Window to become the new active Window in the focus chain for the given
     * virtual @p desktop on the given @p screen.
     *
     * This method makes only sense to use if separate screen focus is used. If separate screen focus
     * is disabled the @p screen is ignored.
     * If no Window for activation is found @c null is returned.
     *
     * @param desktop The virtual desktop to look for a Window for activation
     * @param output The screen to constrain the search on with separate screen focus
     * @return :X11Window *The Window which could be activated or @c null if there is none.
     */
    Window *getForActivation(VirtualDesktop *desktop, Output *output) const;

    /**
     * @brief Checks whether the most recently used focus chain contains the given @p window.
     *
     * Does not consider the per-desktop focus chains.
     * @param window The Window to look for.
     * @return bool @c true if the most recently used focus chain contains @p window, @c false otherwise.
     */
    bool contains(Window *window) const;
    /**
     * @brief Checks whether the focus chain for the given @p desktop contains the given @p window.
     *
     * Does not consider the most recently used focus chain.
     *
     * @param window The Window to look for.
     * @param desktop The virtual desktop whose focus chain should be used
     * @return bool @c true if the focus chain for @p desktop contains @p window, @c false otherwise.
     */
    bool contains(Window *window, VirtualDesktop *desktop) const;
    /**
     * @brief Queries the most recently used focus chain for the next Window after the given
     * @p reference Window.
     *
     * The navigation wraps around the borders of the chain. That is if the @p reference Window is
     * the last item of the focus chain, the first Window will be returned.
     *
     * If the @p reference Window cannot be found in the focus chain, the first element of the focus
     * chain is returned.
     *
     * @param reference The start point in the focus chain to search
     * @return :X11Window *The relatively next Window in the most recently used chain.
     */
    Window *nextMostRecentlyUsed(Window *reference) const;
    /**
     * @brief Queries the focus chain for @p desktop for the next Window in relation to the given
     * @p reference Window.
     *
     * The method finds the first usable Window which is not the @p reference Window. If no Window
     * can be found @c null is returned
     *
     * @param reference The reference Window which should not be returned
     * @param desktop The virtual desktop whose focus chain should be used
     * @return :X11Window *The next usable Window or @c null if none can be found.
     */
    Window *nextForDesktop(Window *reference, VirtualDesktop *desktop) const;
    /**
     * @brief Returns the first Window in the most recently used focus chain. First Window in this
     * case means really the first Window in the chain and not the most recently used Window.
     *
     * @return :X11Window *The first Window in the most recently used chain.
     */
    Window *firstMostRecentlyUsed() const;

    bool isUsableFocusCandidate(Window *window, Window *prev) const;

public Q_SLOTS:
    /**
     * @brief Removes @p window from all focus chains.
     *
     * @param window The Window to remove from all focus chains.
     * @return void
     */
    void remove(KWin::Window *window);
    void setSeparateScreenFocus(bool enabled);
    void setActiveWindow(KWin::Window *window);
    void setCurrentDesktop(VirtualDesktop *desktop);
    void addDesktop(VirtualDesktop *desktop);
    void removeDesktop(VirtualDesktop *desktop);

private:
    using Chain = QList<Window *>;
    /**
     * @brief Makes @p window the first Window in the given focus @p chain.
     *
     * This means the existing position of @p window is dropped and @p window is appended to the
     * @p chain which makes it the first item.
     *
     * @param window The Window to become the first in @p chain
     * @param chain The focus chain to operate on
     * @return void
     */
    void makeFirstInChain(Window *window, Chain &chain);
    /**
     * @brief Makes @p window the last Window in the given focus @p chain.
     *
     * This means the existing position of @p window is dropped and @p window is prepended to the
     * @p chain which makes it the last item.
     *
     * @param window The Window to become the last in @p chain
     * @param chain The focus chain to operate on
     * @return void
     */
    void makeLastInChain(Window *window, Chain &chain);
    void moveAfterWindowInChain(Window *window, Window *reference, Chain &chain);
    void moveBeforeWindowInChain(Window *window, Window *reference, Chain &chain);
    void updateWindowInChain(Window *window, Change change, Chain &chain);
    void insertWindowIntoChain(Window *window, Chain &chain);
    Chain m_mostRecentlyUsed;
    QHash<VirtualDesktop *, Chain> m_desktopFocusChains;
    bool m_separateScreenFocus = false;
    Window *m_activeWindow = nullptr;
    VirtualDesktop *m_currentDesktop = nullptr;
};

inline bool FocusChain::contains(Window *window) const
{
    return m_mostRecentlyUsed.contains(window);
}

inline void FocusChain::setSeparateScreenFocus(bool enabled)
{
    m_separateScreenFocus = enabled;
}

inline void FocusChain::setActiveWindow(Window *window)
{
    m_activeWindow = window;
}

inline void FocusChain::setCurrentDesktop(VirtualDesktop *desktop)
{
    m_currentDesktop = desktop;
}

} // namespace

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "focuschain.h"
#include "window.h"
#include "workspace.h"

namespace KWin
{

void FocusChain::remove(Window *window)
{
    for (auto it = m_desktopFocusChains.begin();
         it != m_desktopFocusChains.end();
         ++it) {
        it.value().removeAll(window);
    }
    m_mostRecentlyUsed.removeAll(window);
}

void FocusChain::addDesktop(VirtualDesktop *desktop)
{
    m_desktopFocusChains.insert(desktop, Chain());
}

void FocusChain::removeDesktop(VirtualDesktop *desktop)
{
    if (m_currentDesktop == desktop) {
        m_currentDesktop = nullptr;
    }
    m_desktopFocusChains.remove(desktop);
}

Window *FocusChain::getForActivation(VirtualDesktop *desktop) const
{
    return getForActivation(desktop, workspace()->activeOutput());
}

Window *FocusChain::getForActivation(VirtualDesktop *desktop, Output *output) const
{
    auto it = m_desktopFocusChains.constFind(desktop);
    if (it == m_desktopFocusChains.constEnd()) {
        return nullptr;
    }
    const auto &chain = it.value();
    for (int i = chain.size() - 1; i >= 0; --i) {
        auto tmp = chain.at(i);
        // TODO: move the check into Window
        if (!tmp->isShade() && tmp->isShown() && tmp->isOnCurrentActivity()
            && (!m_separateScreenFocus || tmp->output() == output)) {
            return tmp;
        }
    }
    return nullptr;
}

void FocusChain::update(Window *window, FocusChain::Change change)
{
    if (!window->wantsTabFocus()) {
        // Doesn't want tab focus, remove
        remove(window);
        return;
    }

    if (window->isOnAllDesktops()) {
        // Now on all desktops, add it to focus chains it is not already in
        for (auto it = m_desktopFocusChains.begin();
             it != m_desktopFocusChains.end();
             ++it) {
            auto &chain = it.value();
            // Making first/last works only on current desktop, don't affect all desktops
            if (it.key() == m_currentDesktop
                && (change == MakeFirst || change == MakeLast)) {
                if (change == MakeFirst) {
                    makeFirstInChain(window, chain);
                } else {
                    makeLastInChain(window, chain);
                }
            } else {
                insertWindowIntoChain(window, chain);
            }
        }
    } else {
        // Now only on desktop, remove it anywhere else
        for (auto it = m_desktopFocusChains.begin();
             it != m_desktopFocusChains.end();
             ++it) {
            auto &chain = it.value();
            if (window->isOnDesktop(it.key())) {
                updateWindowInChain(window, change, chain);
            } else {
                chain.removeAll(window);
            }
        }
    }

    // add for most recently used chain
    updateWindowInChain(window, change, m_mostRecentlyUsed);
}

void FocusChain::updateWindowInChain(Window *window, FocusChain::Change change, Chain &chain)
{
    if (change == MakeFirst) {
        makeFirstInChain(window, chain);
    } else if (change == MakeLast) {
        makeLastInChain(window, chain);
    } else {
        insertWindowIntoChain(window, chain);
    }
}

void FocusChain::insertWindowIntoChain(Window *window, Chain &chain)
{
    if (window->isDeleted()) {
        return;
    }
    if (chain.contains(window)) {
        return;
    }
    if (m_activeWindow && m_activeWindow != window && !chain.empty() && chain.last() == m_activeWindow) {
        // Add it after the active window
        chain.insert(chain.size() - 1, window);
    } else {
        // Otherwise add as the first one
        chain.append(window);
    }
}

void FocusChain::moveAfterWindow(Window *window, Window *reference)
{
    if (window->isDeleted()) {
        return;
    }
    if (!window->wantsTabFocus()) {
        return;
    }
    if (reference == window) {
        return;
    }

    for (auto it = m_desktopFocusChains.begin();
         it != m_desktopFocusChains.end();
         ++it) {
        if (!window->isOnDesktop(it.key())) {
            continue;
        }
        moveAfterWindowInChain(window, reference, it.value());
    }
    moveAfterWindowInChain(window, reference, m_mostRecentlyUsed);
}

void FocusChain::moveBeforeWindow(Window *window, Window *reference)
{
    if (window->isDeleted()) {
        return;
    }
    if (!window->wantsTabFocus()) {
        return;
    }
    if (reference == window) {
        return;
    }

    for (auto it = m_desktopFocusChains.begin();
         it != m_desktopFocusChains.end();
         ++it) {
        if (!window->isOnDesktop(it.key())) {
            continue;
        }
        moveBeforeWindowInChain(window, reference, it.value());
    }
    moveBeforeWindowInChain(window, reference, m_mostRecentlyUsed);
}

void FocusChain::moveAfterWindowInChain(Window *window, Window *reference, Chain &chain)
{
    if (window->isDeleted()) {
        return;
    }
    if (!chain.contains(reference)) {
        return;
    }
    if (Window::belongToSameApplication(reference, window)) {
        chain.removeAll(window);
        chain.insert(chain.indexOf(reference), window);
    } else {
        chain.removeAll(window);
        for (int i = 0; i < chain.size(); ++i) {
            if (Window::belongToSameApplication(reference, chain.at(i))) {
                chain.insert(i, window);
                break;
            }
        }
    }
}

void FocusChain::moveBeforeWindowInChain(Window *window, Window *reference, Chain &chain)
{
    if (window->isDeleted()) {
        return;
    }
    if (!chain.contains(reference)) {
        return;
    }
    if (Window::belongToSameApplication(reference, window)) {
        chain.removeAll(window);
        chain.insert(chain.indexOf(reference) + 1, window);
    } else {
        chain.removeAll(window);
        for (int i = chain.size() - 1; i >= 0; --i) {
            if (Window::belongToSameApplication(reference, chain.at(i))) {
                chain.insert(i + 1, window);
                break;
            }
        }
    }
}

Window *FocusChain::firstMostRecentlyUsed() const
{
    if (m_mostRecentlyUsed.isEmpty()) {
        return nullptr;
    }
    return m_mostRecentlyUsed.first();
}

Window *FocusChain::nextMostRecentlyUsed(Window *reference) const
{
    if (m_mostRecentlyUsed.isEmpty()) {
        return nullptr;
    }
    const int index = m_mostRecentlyUsed.indexOf(reference);
    if (index == -1) {
        return m_mostRecentlyUsed.first();
    }
    if (index == 0) {
        return m_mostRecentlyUsed.last();
    }
    return m_mostRecentlyUsed.at(index - 1);
}

// copied from activation.cpp
bool FocusChain::isUsableFocusCandidate(Window *c, Window *prev) const
{
    return c != prev && !c->isShade() && c->isShown() && c->isOnCurrentDesktop() && c->isOnCurrentActivity() && (!m_separateScreenFocus || c->isOnOutput(prev ? prev->output() : workspace()->activeOutput()));
}

Window *FocusChain::nextForDesktop(Window *reference, VirtualDesktop *desktop) const
{
    auto it = m_desktopFocusChains.constFind(desktop);
    if (it == m_desktopFocusChains.constEnd()) {
        return nullptr;
    }
    const auto &chain = it.value();
    for (int i = chain.size() - 1; i >= 0; --i) {
        auto window = chain.at(i);
        if (isUsableFocusCandidate(window, reference)) {
            return window;
        }
    }
    return nullptr;
}

void FocusChain::makeFirstInChain(Window *window, Chain &chain)
{
    if (window->isDeleted()) {
        return;
    }
    chain.removeAll(window);
    chain.append(window);
}

void FocusChain::makeLastInChain(Window *window, Chain &chain)
{
    if (window->isDeleted()) {
        return;
    }
    chain.removeAll(window);
    chain.prepend(window);
}

bool FocusChain::contains(Window *window, VirtualDesktop *desktop) const
{
    auto it = m_desktopFocusChains.constFind(desktop);
    if (it == m_desktopFocusChains.constEnd()) {
        return false;
    }
    return it.value().contains(window);
}

} // namespace

#include "moc_focuschain.cpp"

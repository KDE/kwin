/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "desktopchain.h"

namespace KWin
{
namespace TabBox
{

DesktopChain::DesktopChain(uint initialSize)
    : m_chain(initialSize)
{
    init();
}

void DesktopChain::init()
{
    for (int i = 0; i < m_chain.size(); ++i) {
        m_chain[i] = i + 1;
    }
}

uint DesktopChain::next(uint indexDesktop) const
{
    const int i = m_chain.indexOf(indexDesktop);
    if (i >= 0 && i + 1 < m_chain.size()) {
        return m_chain[i+1];
    } else if (m_chain.size() > 0) {
        return m_chain[0];
    } else {
        return 1;
    }
}

void DesktopChain::resize(uint previousSize, uint newSize)
{
    Q_ASSERT(int(previousSize) == m_chain.size());
    m_chain.resize(newSize);

    if (newSize >= previousSize) {
        // We do not destroy the chain in case new desktops are added
        for (uint i = previousSize; i < newSize; ++i) {
            m_chain[i] = i + 1;
        }
    } else {
        // But when desktops are removed, we may have to modify the chain a bit,
        // otherwise invalid desktops may show up.
        for (int i = 0; i < m_chain.size(); ++i) {
            m_chain[i] = qMin(m_chain[i], newSize);
        }
    }
}

void DesktopChain::add(uint desktop)
{
    if (m_chain.isEmpty() || int(desktop) > m_chain.count()) {
        return;
    }
    int index = m_chain.indexOf(desktop);
    if (index == -1) {
        // not found - shift all elements by one position
        index = m_chain.size() - 1;
    }
    for (int i = index; i > 0; --i) {
        m_chain[i] = m_chain[i-1];
    }
    m_chain[0] = desktop;
}

DesktopChainManager::DesktopChainManager(QObject *parent)
    : QObject(parent)
    , m_maxChainSize(0)
{
    m_currentChain = m_chains.insert(QString(), DesktopChain());
}

DesktopChainManager::~DesktopChainManager()
{
}

uint DesktopChainManager::next(uint indexDesktop) const
{
    return m_currentChain.value().next(indexDesktop);
}

void DesktopChainManager::resize(uint previousSize, uint newSize)
{
    m_maxChainSize = newSize;
    for (DesktopChains::iterator it = m_chains.begin(); it != m_chains.end(); ++it) {
        it.value().resize(previousSize, newSize);
    }
}

void DesktopChainManager::addDesktop(uint previousDesktop, uint currentDesktop)
{
    Q_UNUSED(previousDesktop)
    m_currentChain.value().add(currentDesktop);
}

void DesktopChainManager::useChain(const QString &identifier)
{
    if (m_currentChain.key().isNull()) {
        createFirstChain(identifier);
    } else {
        m_currentChain = m_chains.find(identifier);
        if (m_currentChain == m_chains.end()) {
            m_currentChain = addNewChain(identifier);
        }
    }
}

void DesktopChainManager::createFirstChain(const QString &identifier)
{
    DesktopChain value(m_currentChain.value());
    m_chains.erase(m_currentChain);
    m_currentChain = m_chains.insert(identifier, value);
}

QHash< QString, DesktopChain >::Iterator DesktopChainManager::addNewChain(const QString &identifier)
{
    return m_chains.insert(identifier, DesktopChain(m_maxChainSize));
}

} // namespace TabBox
} // namespace KWin

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// Qt
#include <QHash>
#include <QObject>
#include <QVector>

namespace KWin
{
namespace TabBox
{

/**
 * @brief A chain for last recently used virtual desktops.
 */
class DesktopChain
{
public:
    /**
     * Creates a last recently used virtual desktop chain with the given @p initialSize.
     */
    explicit DesktopChain(uint initialSize = 0);
    /**
     * Returns the next desktop in the chain starting from @p indexDesktop.
     * In case that the @p indexDesktop is the last desktop of the chain, the method wraps around
     * and returns the first desktop stored in the chain.
     * In case the chain is valid, but does not contain the @p indexDesktop, the first element of
     * the chain is returned.
     * In case the chain is not valid, the always valid virtual desktop with identifier @c 1
     * is returned.
     * @param indexDesktop The id of the virtual desktop which should be used as a starting point
     * @return The next virtual desktop in the chain
     */
    uint next(uint indexDesktop) const;
    /**
     * Adds the @p desktop to the chain. The @p desktop becomes the first element of the
     * chain. All desktops in the chain from the previous index of @p desktop are moved
     * one position in the chain.
     * @param desktop The new desktop to be the top most element in the chain.
     */
    void add(uint desktop);
    /**
     * Resizes the chain from @p previousSize to @p newSize.
     * In case the chain grows new elements are added with a meaning full id in the range
     * [previousSize, newSize].
     * In case the chain shrinks it is ensured that no element points to a virtual desktop
     * with an id larger than @p newSize.
     * @param previousSize The previous size of the desktop chain
     * @param newSize The size to be used for the desktop chain
     */
    void resize(uint previousSize, uint newSize);

private:
    /**
     * Initializes the chain with default values.
     */
    void init();
    QVector<uint> m_chain;
};

/**
 * @brief A manager for multiple desktop chains.
 *
 * This manager keeps track of multiple desktop chains which have a given identifier.
 * A common usage for this is to have a different desktop chain for each Activity.
 */
class DesktopChainManager : public QObject
{
    Q_OBJECT

public:
    explicit DesktopChainManager(QObject *parent = nullptr);
    ~DesktopChainManager() override;

    /**
     * Returns the next virtual desktop starting from @p indexDesktop in the currently used chain.
     * @param indexDesktop The id of the virtual desktop which should be used as a starting point
     * @return The next virtual desktop in the currently used chain
     * @see DesktopChain::next
     */
    uint next(uint indexDesktop) const;

public Q_SLOTS:
    /**
     * Adds the @p currentDesktop to the currently used desktop chain.
     * @param previousDesktop The previously used desktop, should be the top element of the chain
     * @param currentDesktop The desktop which should be the new top element of the chain
     */
    void addDesktop(uint previousDesktop, uint currentDesktop);
    /**
     * Resizes all managed desktop chains from @p previousSize to @p newSize.
     * @param previousSize The previously used size for the chains
     * @param newSize The size to be used for the chains
     * @see DesktopChain::resize
     */
    void resize(uint previousSize, uint newSize);
    /**
     * Switches to the desktop chain identified by the given @p identifier.
     * If there is no chain yet for the given @p identifier a new chain is created and used.
     * @param identifier The identifier of the desktop chain to be used
     */
    void useChain(const QString &identifier);

private:
    typedef QHash<QString, DesktopChain> DesktopChains;
    /**
     * Creates a new desktop chain for the given @p identifier and adds it to the list
     * of identifiers.
     * @returns Position of the new chain in the managed list of chains
     */
    DesktopChains::Iterator addNewChain(const QString &identifier);
    /**
     * Creates the very first list to be used when an @p identifier comes in.
     * The dummy chain which is used by default gets copied and used for this chain.
     */
    void createFirstChain(const QString &identifier);

    DesktopChains::Iterator m_currentChain;
    DesktopChains m_chains;
    /**
     * The maximum size to be used for a new desktop chain
     */
    uint m_maxChainSize;
};

} // TabBox
} // namespace KWin

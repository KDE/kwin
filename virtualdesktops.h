/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_VIRTUAL_DESKTOPS_H
#define KWIN_VIRTUAL_DESKTOPS_H
// KWin
#include <kwinglobals.h>
// Qt includes
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>
// KDE includes
#include <KConfig>
#include <KSharedConfig>

class KLocalizedString;
class NETRootInfo;

namespace KWin {

class VirtualDesktop : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QByteArray id READ id CONSTANT)
    Q_PROPERTY(uint x11DesktopNumber READ x11DesktopNumber CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
public:
    explicit VirtualDesktop(QObject *parent = nullptr);
    virtual ~VirtualDesktop();

    void setId(const QByteArray &id);
    QByteArray id() const {
        return m_id;
    }

    void setName(const QString &name);
    QString name() const {
        return m_name;
    }

    void setX11DesktopNumber(uint number);
    uint x11DesktopNumber() const {
        return m_x11DesktopNumber;
    }

Q_SIGNALS:
    void nameChanged();

private:
    QByteArray m_id;
    QString m_name;
    int m_x11DesktopNumber = 0;

};

/**
 * @brief Two dimensional grid containing the ID of the virtual desktop at a specific position
 * in the grid.
 *
 * The VirtualDesktopGrid represents a visual layout of the Virtual Desktops as they are in e.g.
 * a Pager. This grid is used for getting a desktop next to a given desktop in any direction by
 * making use of the layout information. This allows navigation like move to desktop on left.
 **/
class VirtualDesktopGrid
{
public:
    VirtualDesktopGrid();
    ~VirtualDesktopGrid();
    void update(const QSize &size, Qt::Orientation orientation);
    /**
     * @returns The coords of desktop @a id in grid units.
     */
    QPoint gridCoords(uint id) const;
    /**
     * @returns The ID of the desktop at the point @a coords or 0 if no desktop exists at that
     * point. @a coords is to be in grid units.
     */
    uint at(QPoint coords) const;
    int width() const;
    int height() const;
    const QSize &size() const;
private:
    QSize m_size;
    uint *m_grid;
};

/**
 * @brief Manages the number of available virtual desktops, the layout of those and which virtual
 * desktop is the current one.
 *
 * This manager is responsible for Virtual Desktop handling inside KWin. It has a property for the
 * count of available virtual desktops and a property for the currently active virtual desktop. All
 * changes to the number of virtual desktops and the current virtual desktop need to go through this
 * manager.
 *
 * On all changes a signal is emitted and interested parties should connect to the signal. The manager
 * itself does not interact with other parts of the system. E.g. it does not hide/show windows of
 * desktop changes. This is outside the scope of this manager.
 *
 * Internally the manager organizes the virtual desktops in a grid allowing to navigate over the
 * virtual desktops. For this a set of convenient methods are available which allow to get the id
 * of an adjacent desktop or to switch to an adjacent desktop. Interested parties should make use of
 * these methods and not replicate the logic to switch to the next desktop.
 **/
class VirtualDesktopManager : public QObject
{
    Q_OBJECT
    /**
     * The number of virtual desktops currently available.
     * The ids of the virtual desktops are in the range [1, VirtualDesktopManager::maximum()].
     **/
    Q_PROPERTY(uint count READ count WRITE setCount NOTIFY countChanged)
    /**
     * The id of the virtual desktop which is currently in use.
     **/
    Q_PROPERTY(uint current READ current WRITE setCurrent NOTIFY currentChanged)
    /**
     * Whether navigation in the desktop layout wraps around at the borders.
     **/
    Q_PROPERTY(bool navigationWrappingAround READ isNavigationWrappingAround WRITE setNavigationWrappingAround NOTIFY navigationWrappingAroundChanged)
public:
    virtual ~VirtualDesktopManager();
    /**
     * @internal
     **/
    void setRootInfo(NETRootInfo *info);
    /**
     * @internal
     **/
    void setConfig(KSharedConfig::Ptr config);
    /**
     * @returns Total number of desktops currently in existence.
     * @see setCount
     * @see countChanged
     */
    uint count() const;
    /**
     * @returns The ID of the current desktop.
     * @see setCurrent
     * @see currentChanged
     */
    uint current() const;
    /**
     * Moves to the desktop through the algorithm described by Direction.
     * @param wrap If @c true wraps around to the other side of the layout
     * @see setCurrent
     **/
    template <typename Direction>
    void moveTo(bool wrap = false);

    /**
     * @returns The name of the @p desktop
     **/
    QString name(uint desktop) const;

    /**
     * @returns @c true if navigation at borders of layout wraps around, @c false otherwise
     * @see setNavigationWrappingAround
     * @see navigationWrappingAroundChanged
     **/
    bool isNavigationWrappingAround() const;

    /**
     * @returns The layout aware virtual desktop grid used by this manager.
     **/
    const VirtualDesktopGrid &grid() const;

    /**
     * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
     * the layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint above(uint id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
     * left of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint toRight(uint id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
     * layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint below(uint id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
     * right of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint toLeft(uint id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop after the desktop @a id. Wraps around to the first
     * desktop if @a wrap is set. If @a id is not set use the current desktop.
     **/
    uint next(uint id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop in front of the desktop @a id. Wraps around to the
     * last desktop if @a wrap is set. If @a id is not set use the current desktop.
     **/
    uint previous(uint id = 0, bool wrap = true) const;

    void initShortcuts();

    /**
     * @returns The maximum number of desktops that KWin supports.
     */
    static uint maximum();

public Q_SLOTS:
    /**
     * Set the number of available desktops to @a count. This function overrides any previous
     * grid layout.
     * There needs to be at least one virtual desktop and the new value is capped at the maximum
     * number of desktops. A caller of this function cannot expect that the change has been applied.
     * It is the callers responsibility to either check the @link numberOfDesktops or connect to the
     * @link countChanged signal.
     *
     * In case the @link current desktop is on a desktop higher than the new count, the current desktop
     * is changed to be the new desktop with highest id. In that situation the signal @link desktopsRemoved
     * is emitted.
     * @param count The new number of desktops to use
     * @see count
     * @see maximum
     * @see countChanged
     * @see desktopsRemoved
     */
    void setCount(uint count);
    /**
     * Set the current desktop to @a current.
     * @returns True on success, false otherwise.
     * @see current
     * @see currentChanged
     * @see moveTo
     */
    bool setCurrent(uint current);
    /**
     * Called from within setCount() to ensure the desktop layout is still valid.
     */
    void updateLayout();
    /**
     * @param enable wrapping around borders for navigation in desktop layout
     * @see isNavigationWrappingAround
     * @see navigationWrappingAroundChanged
     **/
    void setNavigationWrappingAround(bool enabled);
    /**
     * Loads number of desktops and names from configuration file
     **/
    void load();
    /**
     * Saves number of desktops and names to configuration file
     **/
    void save();

Q_SIGNALS:
    /**
     * Signal emitted whenever the number of virtual desktops changes.
     * @param previousCount The number of desktops prior to the change
     * @param newCount The new current number of desktops
     **/
    void countChanged(uint previousCount, uint newCount);
    /**
     * Signal emitted whenever the number of virtual desktops changes in a way
     * that existing desktops are removed.
     *
     * The signal is emitted after the @c count property has been updated but prior
     * to the @link countChanged signal being emitted.
     * @param previousCount The number of desktops prior to the change.
     * @see countChanged
     * @see setCount
     * @see count
     **/
    void desktopsRemoved(uint previousCount);
    /**
     * Signal emitted whenever the current desktop changes.
     * @param previousDesktop The virtual desktop changed from
     * @param newDesktop The virtual desktop changed to
     **/
    void currentChanged(uint previousDesktop, uint newDesktop);
    /**
     * Signal emitted whenever the desktop layout changes.
     * @param columns The new number of columns in the layout
     * @param rows The new number of rows in the layout
     **/
    void layoutChanged(int columns, int rows);
    /**
     * Signal emitted whenever the navigationWrappingAround property changes.
     **/
    void navigationWrappingAroundChanged();

private Q_SLOTS:
    /**
     * Common slot for all "Switch to Desktop n" shortcuts.
     * This method uses the sender() method to access some data.
     * DO NOT CALL DIRECTLY! ONLY TO BE USED FROM AN ACTION!
     **/
    void slotSwitchTo();
    /**
     * Slot for switch to next desktop action.
     **/
    void slotNext();
    /**
     * Slot for switch to previous desktop action.
     **/
    void slotPrevious();
    /**
     * Slot for switch to right desktop action.
     **/
    void slotRight();
    /**
     * Slot for switch to left desktop action.
     **/
    void slotLeft();
    /**
     * Slot for switch to desktop above action.
     **/
    void slotUp();
    /**
     * Slot for switch to desktop below action.
     **/
    void slotDown();

private:
    /**
     * This method is called when the number of desktops is updated in a way that desktops
     * are removed. At the time when this method is invoked the count property is already
     * updated but the corresponding signal has not been emitted yet.
     *
     * Ensures that in case the current desktop is on one of the removed
     * desktops the last desktop after the change becomes the new desktop.
     * Emits the signal @link desktopsRemoved.
     *
     * @param previousCount The number of desktops prior to the change.
     * @param previousCurrent The number of the previously current desktop.
     * @see setCount
     * @see desktopsRemoved
     **/
    void handleDesktopsRemoved(uint previousCount, uint previousCurrent);
    /**
     * Generate a desktop layout from EWMH _NET_DESKTOP_LAYOUT property parameters.
     */
    void setNETDesktopLayout(Qt::Orientation orientation, uint width, uint height, int startingCorner);
    /**
     * Updates the net root info for new number of desktops
     **/
    void updateRootInfo();
    /**
     * @returns A default name for the given @p desktop
     **/
    QString defaultName(int desktop) const;
    /**
     * Creates all the global keyboard shortcuts for "Switch To Desktop n" actions.
     **/
    void initSwitchToShortcuts();
    /**
     * Creates an action and connects it to the @p slot in this Manager. This method is
     * meant to be used for the case that an additional information needs to be stored in
     * the action and the label.
     * @param name The name of the action to be created
     * @param label The localized name for the action to be created
     * @param value An additional value added to the label and to the created action
     * @param key The global shortcut for the action
     * @param slot The slot to invoke when the action is triggered
     **/
    void addAction(const QString &name, const KLocalizedString &label, uint value, const QKeySequence &key, void (VirtualDesktopManager::*slot)());
    /**
     * Creates an action and connects it to the @p slot in this Manager.
     * Overloaded method for the case that no additional value needs to be passed to the action and
     * no global shortcut is defined by default.
     * @param name The name of the action to be created
     * @param label The localized name for the action to be created
     * @param slot The slot to invoke when the action is triggered
     **/
    void addAction(const QString &name, const QString &label, void (VirtualDesktopManager::*slot)());

    QVector<VirtualDesktop*> m_desktops;
    QPointer<VirtualDesktop> m_current;
    bool m_navigationWrapsAround;
    VirtualDesktopGrid m_grid;
    // TODO: QPointer
    NETRootInfo *m_rootInfo;
    KSharedConfig::Ptr m_config;

    KWIN_SINGLETON_VARIABLE(VirtualDesktopManager, s_manager)
};

/**
 * Function object to select the desktop above in the layout.
 * Note: does not switch to the desktop!
 **/
class DesktopAbove
{
public:
    DesktopAbove() {}
    /**
     * @param desktop The desktop from which the desktop above should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already topmost desktop
     * @returns Id of the desktop above @p desktop
     **/
    uint operator() (uint desktop, bool wrap) {
        return VirtualDesktopManager::self()->above(desktop, wrap);
    }
};

/**
 * Function object to select the desktop below in the layout.
 * Note: does not switch to the desktop!
 **/
class DesktopBelow
{
public:
    DesktopBelow() {}
    /**
     * @param desktop The desktop from which the desktop below should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already lowest desktop
     * @returns Id of the desktop below @p desktop
     **/
    uint operator() (uint desktop, bool wrap) {
        return VirtualDesktopManager::self()->below(desktop, wrap);
    }
};

/**
 * Function object to select the desktop to the left in the layout.
 * Note: does not switch to the desktop!
 **/
class DesktopLeft
{
public:
    DesktopLeft() {}
    /**
     * @param desktop The desktop from which the desktop on the left should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already leftmost desktop
     * @returns Id of the desktop left of @p desktop
     **/
    uint operator() (uint desktop, bool wrap) {
        return VirtualDesktopManager::self()->toLeft(desktop, wrap);
    }
};

/**
 * Function object to select the desktop to the right in the layout.
 * Note: does not switch to the desktop!
 **/
class DesktopRight
{
public:
    DesktopRight() {}
    /**
     * @param desktop The desktop from which the desktop on the right should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already rightmost desktop
     * @returns Id of the desktop right of @p desktop
     **/
    uint operator() (uint desktop, bool wrap) {
        return VirtualDesktopManager::self()->toRight(desktop, wrap);
    }
};

/**
 * Function object to select the next desktop in the layout.
 * Note: does not switch to the desktop!
 **/
class DesktopNext
{
public:
    DesktopNext() {}
    /**
     * @param desktop The desktop from which the next desktop should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already last desktop
     * @returns Id of the next desktop
     **/
    uint operator() (uint desktop, bool wrap) {
        return VirtualDesktopManager::self()->next(desktop, wrap);
    }
};

/**
 * Function object to select the previous desktop in the layout.
 * Note: does not switch to the desktop!
 **/
class DesktopPrevious
{
public:
    DesktopPrevious() {}
    /**
     * @param desktop The desktop from which the previous desktop should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already first desktop
     * @returns Id of the previous desktop
     **/
    uint operator() (uint desktop, bool wrap) {
        return VirtualDesktopManager::self()->previous(desktop, wrap);
    }
};

/**
 * Helper function to get the ID of a virtual desktop in the direction from
 * the given @p desktop. If @c 0 the current desktop is used as a starting point.
 * @param desktop The desktop from which the desktop in given Direction should be selected.
 * @param wrap Whether desktop navigation wraps around at the borders of the layout
 * @returns The next desktop in specified direction
 **/
template <typename Direction>
uint getDesktop(int desktop = 0, bool wrap = true);

template <typename Direction>
uint getDesktop(int d, bool wrap)
{
    Direction direction;
    return direction(d, wrap);
}

inline
int VirtualDesktopGrid::width() const
{
    return m_size.width();
}

inline
int VirtualDesktopGrid::height() const
{
    return m_size.height();
}

inline
const QSize &VirtualDesktopGrid::size() const
{
    return m_size;
}

inline
uint VirtualDesktopGrid::at(QPoint coords) const
{
    const int index = coords.y() * m_size.width() + coords.x();
    if (index > m_size.width() * m_size.height() || coords.x() >= width() || coords.y() >= height()) {
        return 0;
    }
    return m_grid[index];
}

inline
uint VirtualDesktopManager::maximum()
{
    return 20;
}

inline
uint VirtualDesktopManager::count() const
{
    return m_desktops.count();
}

inline
bool VirtualDesktopManager::isNavigationWrappingAround() const
{
    return m_navigationWrapsAround;
}

inline
void VirtualDesktopManager::setRootInfo(NETRootInfo *info)
{
    m_rootInfo = info;
}

inline
void VirtualDesktopManager::setConfig(KSharedConfig::Ptr config)
{
    m_config = config;
}

inline
const VirtualDesktopGrid &VirtualDesktopManager::grid() const
{
    return m_grid;
}

template <typename Direction>
void VirtualDesktopManager::moveTo(bool wrap)
{
    Direction functor;
    setCurrent(functor(0, wrap));
}

} // namespace KWin
#endif

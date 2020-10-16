/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_VIRTUAL_DESKTOPS_H
#define KWIN_VIRTUAL_DESKTOPS_H
// KWin
#include <kwinglobals.h>
#include <kwin_export.h>
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
class QAction;

namespace KWaylandServer
{
class PlasmaVirtualDesktopManagementInterface;
}

namespace KWin {

class KWIN_EXPORT VirtualDesktop : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QByteArray id READ id CONSTANT)
    Q_PROPERTY(uint x11DesktopNumber READ x11DesktopNumber NOTIFY x11DesktopNumberChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
public:
    explicit VirtualDesktop(QObject *parent = nullptr);
    ~VirtualDesktop() override;

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
    void x11DesktopNumberChanged();
    /**
     * Emitted just before the desktop gets destroyed.
     */
    void aboutToBeDestroyed();

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
 */
class VirtualDesktopGrid
{
public:
    VirtualDesktopGrid();
    ~VirtualDesktopGrid();
    void update(const QSize &size, Qt::Orientation orientation, const QVector<VirtualDesktop*> &desktops);
    /**
     * @returns The coords of desktop @a id in grid units.
     */
    QPoint gridCoords(uint id) const;
    /**
     * @returns The coords of desktop @a vd in grid units.
     */
    QPoint gridCoords(VirtualDesktop *vd) const;
    /**
     * @returns The desktop at the point @a coords or 0 if no desktop exists at that
     * point. @a coords is to be in grid units.
     */
    VirtualDesktop *at(const QPoint &coords) const;
    int width() const;
    int height() const;
    const QSize &size() const;
private:
    QSize m_size;
    QVector<QVector<VirtualDesktop*>> m_grid;
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
 */
class KWIN_EXPORT VirtualDesktopManager : public QObject
{
    Q_OBJECT
    /**
     * The number of virtual desktops currently available.
     * The ids of the virtual desktops are in the range [1, VirtualDesktopManager::maximum()].
     */
    Q_PROPERTY(uint count READ count WRITE setCount NOTIFY countChanged)
    /**
     * The id of the virtual desktop which is currently in use.
     */
    Q_PROPERTY(uint current READ current WRITE setCurrent NOTIFY currentChanged)
    /**
     * Whether navigation in the desktop layout wraps around at the borders.
     */
    Q_PROPERTY(bool navigationWrappingAround READ isNavigationWrappingAround WRITE setNavigationWrappingAround NOTIFY navigationWrappingAroundChanged)
public:
    ~VirtualDesktopManager() override;
    /**
     * @internal, for X11 case
     */
    void setRootInfo(NETRootInfo *info);
    /**
     * @internal, for Wayland case
     */
    void setVirtualDesktopManagement(KWaylandServer::PlasmaVirtualDesktopManagementInterface *management);
    /**
     * @internal
     */
    void setConfig(KSharedConfig::Ptr config);
    /**
     * @returns Total number of desktops currently in existence.
     * @see setCount
     * @see countChanged
     */
    uint count() const;
    /**
     * @returns the number of rows the layout has.
     * @see setRows
     * @see rowsChanged
     */
    uint rows() const;
    /**
     * @returns The ID of the current desktop.
     * @see setCurrent
     * @see currentChanged
     */
    uint current() const;
    /**
     * @returns The current desktop
     * @see setCurrent
     * @see currentChanged
     */
    VirtualDesktop *currentDesktop() const;
    /**
     * Moves to the desktop through the algorithm described by Direction.
     * @param wrap If @c true wraps around to the other side of the layout
     * @see setCurrent
     */
    template <typename Direction>
    void moveTo(bool wrap = false);

    /**
     * @returns The name of the @p desktop
     */
    QString name(uint desktop) const;

    /**
     * @returns @c true if navigation at borders of layout wraps around, @c false otherwise
     * @see setNavigationWrappingAround
     * @see navigationWrappingAroundChanged
     */
    bool isNavigationWrappingAround() const;

    /**
     * @returns The layout aware virtual desktop grid used by this manager.
     */
    const VirtualDesktopGrid &grid() const;

    /**
     * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
     * the layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint above(uint id = 0, bool wrap = true) const;
    /**
     * @returns The desktop above desktop @a desktop. Wraps around to the bottom of
     * the layout if @a wrap is set. If @a desktop is @c null use the current one.
     */
    VirtualDesktop *above(VirtualDesktop *desktop, bool wrap = true) const;
    /**
     * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
     * left of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint toRight(uint id = 0, bool wrap = true) const;
    /**
     * @returns The desktop to the right of desktop @a desktop. Wraps around to the
     * left of the layout if @a wrap is set. If @a desktop is @c null use the current one.
     */
    VirtualDesktop *toRight(VirtualDesktop *desktop, bool wrap = true) const;
    /**
     * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
     * layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint below(uint id = 0, bool wrap = true) const;
    /**
     * @returns The desktop below desktop @a desktop. Wraps around to the top of the
     * layout if @a wrap is set. If @a desktop is @c null use the current one.
     */
    VirtualDesktop *below(VirtualDesktop *desktop, bool wrap = true) const;
    /**
     * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
     * right of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    uint toLeft(uint id = 0, bool wrap = true) const;
    /**
     * @returns The desktop to the left of desktop @a desktop. Wraps around to the
     * right of the layout if @a wrap is set. If @a desktop is @c null use the current one.
     */
    VirtualDesktop *toLeft(VirtualDesktop *desktop, bool wrap = true) const;
    /**
     * @returns The desktop after the desktop @a desktop. Wraps around to the first
     * desktop if @a wrap is set. If @a desktop is @c null use the current desktop.
     */
    VirtualDesktop *next(VirtualDesktop *desktop = nullptr, bool wrap = true) const;
    /**
     * @returns The desktop in front of the desktop @a desktop. Wraps around to the
     * last desktop if @a wrap is set. If @a desktop is @c null use the current desktop.
     */
    VirtualDesktop *previous(VirtualDesktop *desktop = nullptr, bool wrap = true) const;

    void initShortcuts();

    /**
     * @returns all currently managed VirtualDesktops
     */
    QVector<VirtualDesktop*> desktops() const {
        return m_desktops;
    }

    /**
     * @returns The VirtualDesktop for the x11 @p id, if no such VirtualDesktop @c null is returned
     */
    VirtualDesktop *desktopForX11Id(uint id) const;

    /**
     * @returns The VirtualDesktop for the internal desktop string @p id, if no such VirtualDesktop @c null is returned
     */
    VirtualDesktop *desktopForId(const QByteArray &id) const;

    /**
     * Create a new virtual desktop at the requested position.
     * The difference with setCount is that setCount always adds new desktops at the end of the chain. The Id is automatically generated.
     * @param position The position of the desktop. It should be in range [0, count].
     * @param name The name for the new desktop, if empty the default name will be used.
     * @returns the new VirtualDesktop, nullptr if we reached the maximum number of desktops
     */
     VirtualDesktop *createVirtualDesktop(uint position, const QString &name = QString());

    /**
     * Remove the virtual desktop identified by id, if it exists
     * difference with setCount is that is possible to remove an arbitrary desktop,
     * not only the last one.
     * @param id the string id of the desktop to remove
     */
    void removeVirtualDesktop(const QByteArray &id);

    /**
     * Updates the net root info for new number of desktops
     */
    void updateRootInfo();

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
     * It is the callers responsibility to either check the numberOfDesktops or connect to the
     * countChanged signal.
     *
     * In case the @ref current desktop is on a desktop higher than the new count, the current desktop
     * is changed to be the new desktop with highest id. In that situation the signal desktopRemoved
     * is emitted.
     * @param count The new number of desktops to use
     * @see count
     * @see maximum
     * @see countChanged
     * @see desktopCreated
     * @see desktopRemoved
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
     * Set the current desktop to @a current.
     * @returns True on success, false otherwise.
     * @see current
     * @see currentChanged
     * @see moveTo
     */
    bool setCurrent(VirtualDesktop *current);
    /**
     * Updates the layout to a new number of rows. The number of columns will be calculated accordingly
     */
    void setRows(uint rows);
    /**
     * Called from within setCount() to ensure the desktop layout is still valid.
     */
    void updateLayout();
    /**
     * @param enabled wrapping around borders for navigation in desktop layout
     * @see isNavigationWrappingAround
     * @see navigationWrappingAroundChanged
     */
    void setNavigationWrappingAround(bool enabled);
    /**
     * Loads number of desktops and names from configuration file
     */
    void load();
    /**
     * Saves number of desktops and names to configuration file
     */
    void save();

Q_SIGNALS:
    /**
     * Signal emitted whenever the number of virtual desktops changes.
     * @param previousCount The number of desktops prior to the change
     * @param newCount The new current number of desktops
     */
    void countChanged(uint previousCount, uint newCount);

    /**
     * Signal when the number of rows in the layout changes
     * @param rows number of rows
     */
    void rowsChanged(uint rows);

    /**
     * A new desktop has been created
     * @param desktop the new just crated desktop
     */
    void desktopCreated(KWin::VirtualDesktop *desktop);

    /**
     * A desktop has been removed and is about to be deleted
     * @param desktop the desktop that has been removed.
     *          It's guaranteed to stil la valid pointer when the signal arrives,
     *          but it's about to be deleted.
     */
    void desktopRemoved(KWin::VirtualDesktop *desktop);

    /**
     * Signal emitted whenever the current desktop changes.
     * @param previousDesktop The virtual desktop changed from
     * @param newDesktop The virtual desktop changed to
     */
    void currentChanged(uint previousDesktop, uint newDesktop);
    /**
     * Signal emitted whenever the desktop layout changes.
     * @param columns The new number of columns in the layout
     * @param rows The new number of rows in the layout
     */
    void layoutChanged(int columns, int rows);
    /**
     * Signal emitted whenever the navigationWrappingAround property changes.
     */
    void navigationWrappingAroundChanged();

private Q_SLOTS:
    /**
     * Common slot for all "Switch to Desktop n" shortcuts.
     * This method uses the sender() method to access some data.
     * DO NOT CALL DIRECTLY! ONLY TO BE USED FROM AN ACTION!
     */
    void slotSwitchTo();
    /**
     * Slot for switch to next desktop action.
     */
    void slotNext();
    /**
     * Slot for switch to previous desktop action.
     */
    void slotPrevious();
    /**
     * Slot for switch to right desktop action.
     */
    void slotRight();
    /**
     * Slot for switch to left desktop action.
     */
    void slotLeft();
    /**
     * Slot for switch to desktop above action.
     */
    void slotUp();
    /**
     * Slot for switch to desktop below action.
     */
    void slotDown();

private:
    /**
     * Generate a desktop layout from EWMH _NET_DESKTOP_LAYOUT property parameters.
     */
    void setNETDesktopLayout(Qt::Orientation orientation, uint width, uint height, int startingCorner);
    /**
     * @returns A default name for the given @p desktop
     */
    QString defaultName(int desktop) const;
    /**
     * Creates all the global keyboard shortcuts for "Switch To Desktop n" actions.
     */
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
     */
    QAction *addAction(const QString &name, const KLocalizedString &label, uint value, const QKeySequence &key, void (VirtualDesktopManager::*slot)());
    /**
     * Creates an action and connects it to the @p slot in this Manager.
     * Overloaded method for the case that no additional value needs to be passed to the action and
     * no global shortcut is defined by default.
     * @param name The name of the action to be created
     * @param label The localized name for the action to be created
     * @param slot The slot to invoke when the action is triggered
     */
    QAction *addAction(const QString &name, const QString &label, void (VirtualDesktopManager::*slot)());

    QVector<VirtualDesktop*> m_desktops;
    QPointer<VirtualDesktop> m_current;
    quint32 m_rows = 2;
    bool m_navigationWrapsAround;
    VirtualDesktopGrid m_grid;
    // TODO: QPointer
    NETRootInfo *m_rootInfo;
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *m_virtualDesktopManagement = nullptr;
    KSharedConfig::Ptr m_config;

    KWIN_SINGLETON_VARIABLE(VirtualDesktopManager, s_manager)
};

/**
 * Function object to select the desktop above in the layout.
 * Note: does not switch to the desktop!
 */
class DesktopAbove
{
public:
    DesktopAbove() {}
    /**
     * @param desktop The desktop from which the desktop above should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already topmost desktop
     * @returns Id of the desktop above @p desktop
     */
    uint operator() (uint desktop, bool wrap) {
        return (*this)(VirtualDesktopManager::self()->desktopForX11Id(desktop), wrap)->x11DesktopNumber();
    }
    /**
     * @param desktop The desktop from which the desktop above should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already topmost desktop
     * @returns the desktop above @p desktop
     */
    VirtualDesktop *operator() (VirtualDesktop *desktop, bool wrap) {
        return VirtualDesktopManager::self()->above(desktop, wrap);
    }
};

/**
 * Function object to select the desktop below in the layout.
 * Note: does not switch to the desktop!
 */
class DesktopBelow
{
public:
    DesktopBelow() {}
    /**
     * @param desktop The desktop from which the desktop below should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already lowest desktop
     * @returns Id of the desktop below @p desktop
     */
    uint operator() (uint desktop, bool wrap) {
        return (*this)(VirtualDesktopManager::self()->desktopForX11Id(desktop), wrap)->x11DesktopNumber();
    }
    /**
     * @param desktop The desktop from which the desktop below should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already lowest desktop
     * @returns the desktop below @p desktop
     */
    VirtualDesktop *operator() (VirtualDesktop *desktop, bool wrap) {
        return VirtualDesktopManager::self()->below(desktop, wrap);
    }
};

/**
 * Function object to select the desktop to the left in the layout.
 * Note: does not switch to the desktop!
 */
class DesktopLeft
{
public:
    DesktopLeft() {}
    /**
     * @param desktop The desktop from which the desktop on the left should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already leftmost desktop
     * @returns Id of the desktop left of @p desktop
     */
    uint operator() (uint desktop, bool wrap) {
        return (*this)(VirtualDesktopManager::self()->desktopForX11Id(desktop), wrap)->x11DesktopNumber();
    }
    /**
     * @param desktop The desktop from which the desktop on the left should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already leftmost desktop
     * @returns the desktop left of @p desktop
     */
    VirtualDesktop *operator() (VirtualDesktop *desktop, bool wrap) {
        return VirtualDesktopManager::self()->toLeft(desktop, wrap);
    }
};

/**
 * Function object to select the desktop to the right in the layout.
 * Note: does not switch to the desktop!
 */
class DesktopRight
{
public:
    DesktopRight() {}
    /**
     * @param desktop The desktop from which the desktop on the right should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already rightmost desktop
     * @returns Id of the desktop right of @p desktop
     */
    uint operator() (uint desktop, bool wrap) {
        return (*this)(VirtualDesktopManager::self()->desktopForX11Id(desktop), wrap)->x11DesktopNumber();
    }
    /**
     * @param desktop The desktop from which the desktop on the right should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already rightmost desktop
     * @returns the desktop right of @p desktop
     */
    VirtualDesktop *operator() (VirtualDesktop *desktop, bool wrap) {
        return VirtualDesktopManager::self()->toRight(desktop, wrap);
    }
};

/**
 * Function object to select the next desktop in the layout.
 * Note: does not switch to the desktop!
 */
class DesktopNext
{
public:
    DesktopNext() {}
    /**
     * @param desktop The desktop from which the next desktop should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already last desktop
     * @returns Id of the next desktop
     */
    uint operator() (uint desktop, bool wrap) {
        return (*this)(VirtualDesktopManager::self()->desktopForX11Id(desktop), wrap)->x11DesktopNumber();
    }
    /**
     * @param desktop The desktop from which the next desktop should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already last desktop
     * @returns the next desktop
     */
    VirtualDesktop *operator() (VirtualDesktop *desktop, bool wrap) {
        return VirtualDesktopManager::self()->next(desktop, wrap);
    }
};

/**
 * Function object to select the previous desktop in the layout.
 * Note: does not switch to the desktop!
 */
class DesktopPrevious
{
public:
    DesktopPrevious() {}
    /**
     * @param desktop The desktop from which the previous desktop should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already first desktop
     * @returns Id of the previous desktop
     */
    uint operator() (uint desktop, bool wrap) {
        return (*this)(VirtualDesktopManager::self()->desktopForX11Id(desktop), wrap)->x11DesktopNumber();
    }
    /**
     * @param desktop The desktop from which the previous desktop should be selected. If @c 0 the current desktop is used
     * @param wrap Whether to wrap around if already first desktop
     * @returns the previous desktop
     */
    VirtualDesktop *operator() (VirtualDesktop *desktop, bool wrap) {
        return VirtualDesktopManager::self()->previous(desktop, wrap);
    }
};

/**
 * Helper function to get the ID of a virtual desktop in the direction from
 * the given @p desktop. If @c 0 the current desktop is used as a starting point.
 * @param desktop The desktop from which the desktop in given Direction should be selected.
 * @param wrap Whether desktop navigation wraps around at the borders of the layout
 * @returns The next desktop in specified direction
 */
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
void VirtualDesktopManager::setConfig(KSharedConfig::Ptr config)
{
    m_config = std::move(config);
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
    setCurrent(functor(nullptr, wrap));
}

} // namespace KWin
#endif

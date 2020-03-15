/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_POINTERCONSTRAINTS_INTERFACE_H
#define KWAYLAND_SERVER_POINTERCONSTRAINTS_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

#include <QRegion>

namespace KWayland
{
namespace Server
{

class Display;
class SurfaceInterface;

/**
 * Enum describing the interface versions the PointerConstraintsInterface can support.
 *
 * @since 5.29
 **/
enum class PointerConstraintsInterfaceVersion {
    /**
     * zwp_pointer_constraints_v1
     **/
     UnstableV1
};

/**
 * Manager object to create pointer constraints.
 *
 * To create this manager use {@link Display::createPointerConstraints}
 *
 * @see ConfinedPointerInterface
 * @see LockedPointerInterface
 * @see Display::createPointerConstraints
 * @since 5.29
 **/
class KWAYLANDSERVER_EXPORT PointerConstraintsInterface : public Global
{
    Q_OBJECT
public:
    virtual ~PointerConstraintsInterface();

    /**
     * @returns The interface version used by this PointerConstraintsInterface
     **/
    PointerConstraintsInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit PointerConstraintsInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

/**
 * The LockedPointerInterface lets the client request to disable movements of
 * the virtual pointer (i.e. the cursor), effectively locking the pointer
 * to a position.
 *
 * It is up to the compositor whether the lock gets activated.
 * To activate it needs to use {@link LockedPointerInterface::setLocked}.
 * The compositor needs to ensure that the SurfaceInterface has pointer focus
 * and that the pointer is inside the {@link LockedPointerInterface::region} when
 * it activates the lock.
 *
 * While the lock is active the PointerInterface does no longer emit pointer motion
 * events, but still emits relative pointer motion events.
 *
 * @since 5.29
 **/
class KWAYLANDSERVER_EXPORT LockedPointerInterface : public Resource
{
    Q_OBJECT
public:

    virtual ~LockedPointerInterface();

    /**
     * @returns The interface version used by this LockedPointerInterface
     **/
    PointerConstraintsInterfaceVersion interfaceVersion() const;

    enum class LifeTime {
        OneShot,
        Persistent
    };

    LifeTime lifeTime() const;

    /**
     * The intersection of this region and the input region of the SurfaceInterface is used
     * to determine where the pointer must be in order for the lock to activate.
     * It is up to the compositor whether to warp the pointer or require some kind of
     * user interaction for the lock to activate.
     *
     * If the region is empty the SurfaceInterface input region is used.
     *
     * @see regionChanged
     * @see SurfaceInterface::input
     **/
    QRegion region() const;

    /**
     * Indicates where the mouse cursor should be positioned after it has been unlocked again.
     * The compositor can warp the cursor at this moment to the position. For that it
     * will not emit any relative motion events. The hint is relative to the top-left
     * corner of the surface the lock was applied to. Only non-negative x and y values
     * are allowed. Otherwise the hint is invalid and should be ignored by the compositor.
     *
     * In case the client never set the hint, an invalid one will be returned.
     *
     * This function should be called when the compositor decides to break the lock or the
     * client unbinds the resource. To set the position in this case the compositor should
     * call this function when the aboutToBeUnbound signal has been emitted.
     *
     * @see cursorPositionHintChanged
     * @since 5.49
     **/
    QPointF cursorPositionHint() const;

    /**
     * Whether the Compositor set this pointer lock to be active.
     * @see setLocked
     * @see lockedChanged
     **/
    bool isLocked() const;

    /**
     * Activates or deactivates the lock.
     *
     * A pointer lock can only be activated if the SurfaceInterface
     * this LockedPointerInterface was created for has pointer focus
     * and the pointer is inside the {@link region}.
     *
     * Unlocking resets the cursor position hint.
     *
     * @param locked Whether the lock should be active
     * @see isLocked
     * @see lockedChanged
     **/
    void setLocked(bool locked);

Q_SIGNALS:
    /**
     * Emitted whenever the region changes.
     * This happens when the parent SurfaceInterface gets committed
     * @see region
     **/
    void regionChanged();

    /**
     * Emitted whenever the cursor position hint changes.
     * This happens when the parent SurfaceInterface gets committed
     * @see cursorPositionHint
     * @since 5.49
     **/
    void cursorPositionHintChanged();

    /**
     * Emitted whenever the {@link isLocked} state changes.
     * @see isLocked
     * @see setLocked
     **/
    void lockedChanged();

protected:
    class Private;
    explicit LockedPointerInterface(Private *p, QObject *parent = nullptr);

private:
    Private *d_func() const;
    friend class SurfaceInterface;
};

/**
 *
 * The ConfinedPointerInterface gets installed on a SurfaceInterface.
 * The confinement indicates that the SurfaceInterface wants to confine the
 * pointer to a region of the SurfaceInterface.
 *
 * It is up to the compositor whether the confinement gets activated.
 * To activate it needs to use {@link ConfinedPointerInterface::setConfined}.
 * The compositor needs to ensure that the SurfaceInterface has pointer focus
 * and that the pointer is inside the {@link ConfinedPointerInterface::region} when
 * it activates the confinement.
 *
 * From client side the confinement gets deactivated by destroying the ConfinedPointerInterface.
 * From compositor side the confinement can be deactivated by setting
 * {@link ConfinedPointerInterface::setConfined} to @c false.
 *
 * @since 5.29
 **/
class KWAYLANDSERVER_EXPORT ConfinedPointerInterface : public Resource
{
    Q_OBJECT
public:

    virtual ~ConfinedPointerInterface();

    /**
     * @returns The interface version used by this ConfinedPointerInterface
     **/
    PointerConstraintsInterfaceVersion interfaceVersion() const;

    enum class LifeTime {
        OneShot,
        Persistent
    };

    LifeTime lifeTime() const;

    /**
     * The intersection of this region and the input region of the SurfaceInterface is used
     * to determine where the pointer must be in order for the confinement to activate.
     * It is up to the compositor whether to warp the pointer or require some kind of
     * user interaction for the confinement to activate.
     *
     * If the region is empty the SurfaceInterface input region is used.
     *
     * @see regionChanged
     * @see SurfaceInterface::input
     **/
    QRegion region() const;

    /**
     * Whether the Compositor set this pointer confinement to be active.
     * @see setConfined
     * @see confinedChanged
     **/
    bool isConfined() const;

    /**
     * Activates or deactivates the confinement.
     *
     * A pointer confinement can only be activated if the SurfaceInterface
     * this ConfinedPointerInterface was created for has pointer focus
     * and the pointer is inside the {@link region}.
     *
     * @param confined Whether the confinement should be active
     * @see isConfined
     * @see confinedChanged
     **/
    void setConfined(bool confined);

Q_SIGNALS:
    /**
     * Emitted whenever the region changes.
     * This happens when the parent SurfaceInterface gets committed
     * @see region
     **/
    void regionChanged();

    /**
     * Emitted whenever the {@link isConfined} state changes.
     * @see isConfined
     * @see setConfined
     **/
    void confinedChanged();

protected:
    class Private;
    explicit ConfinedPointerInterface(Private *p, QObject *parent = nullptr);

private:
    Private *d_func() const;
    friend class SurfaceInterface;
};

}
}

#endif

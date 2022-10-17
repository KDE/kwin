/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QRegion>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class ConfinedPointerV1InterfacePrivate;
class Display;
class LockedPointerV1InterfacePrivate;
class PointerConstraintsV1InterfacePrivate;
class SurfaceInterface;

/**
 * Manager object to create pointer constraints.
 *
 * To create this manager use {@link Display::createPointerConstraintsV1}
 *
 * @see ConfinedPointerV1Interface
 * @see LockedPointerV1Interface
 */
class KWIN_EXPORT PointerConstraintsV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit PointerConstraintsV1Interface(Display *display, QObject *parent = nullptr);
    ~PointerConstraintsV1Interface() override;

private:
    std::unique_ptr<PointerConstraintsV1InterfacePrivate> d;
};

/**
 * The LockedPointerV1Interface lets the client request to disable movements of
 * the virtual pointer (i.e. the cursor), effectively locking the pointer
 * to a position.
 *
 * It is up to the compositor whether the lock gets activated.
 * To activate it needs to use {@link LockedPointerV1Interface::setLocked}.
 * The compositor needs to ensure that the SurfaceInterface has pointer focus
 * and that the pointer is inside the {@link LockedPointerV1Interface::region} when
 * it activates the lock.
 *
 * While the lock is active the PointerInterface does no longer Q_EMIT pointer motion
 * events, but still emits relative pointer motion events.
 */
class KWIN_EXPORT LockedPointerV1Interface : public QObject
{
    Q_OBJECT

public:
    ~LockedPointerV1Interface() override;

    enum class LifeTime : uint {
        OneShot = 1,
        Persistent = 2,
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
     */
    QRegion region() const;

    /**
     * Indicates where the mouse cursor should be positioned after it has been unlocked again.
     * The compositor can warp the cursor at this moment to the position. For that it
     * will not Q_EMIT any relative motion events. The hint is relative to the top-left
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
     */
    QPointF cursorPositionHint() const;

    /**
     * Whether the Compositor set this pointer lock to be active.
     * @see setLocked
     * @see lockedChanged
     */
    bool isLocked() const;

    /**
     * Activates or deactivates the lock.
     *
     * A pointer lock can only be activated if the SurfaceInterface
     * this LockedPointerV1Interface was created for has pointer focus
     * and the pointer is inside the {@link region}.
     *
     * Unlocking resets the cursor position hint.
     *
     * @param locked Whether the lock should be active
     * @see isLocked
     * @see lockedChanged
     */
    void setLocked(bool locked);

Q_SIGNALS:
    /**
     * This is signal is emitted when the locked pointer is about to be destroyed.
     */
    void aboutToBeDestroyed();

    /**
     * Emitted whenever the region changes.
     * This happens when the parent SurfaceInterface gets committed
     * @see region
     */
    void regionChanged();

    /**
     * Emitted whenever the cursor position hint changes.
     * This happens when the parent SurfaceInterface gets committed
     * @see cursorPositionHint
     */
    void cursorPositionHintChanged();

    /**
     * Emitted whenever the {@link isLocked} state changes.
     * @see isLocked
     * @see setLocked
     */
    void lockedChanged();

private:
    LockedPointerV1Interface(SurfaceInterface *surface, LifeTime lifeTime, const QRegion &region, ::wl_resource *resource);
    std::unique_ptr<LockedPointerV1InterfacePrivate> d;
    friend class LockedPointerV1InterfacePrivate;
    friend class PointerConstraintsV1InterfacePrivate;
};

/**
 *
 * The ConfinedPointerV1Interface gets installed on a SurfaceInterface.
 * The confinement indicates that the SurfaceInterface wants to confine the
 * pointer to a region of the SurfaceInterface.
 *
 * It is up to the compositor whether the confinement gets activated.
 * To activate it needs to use {@link ConfinedPointerV1Interface::setConfined}.
 * The compositor needs to ensure that the SurfaceInterface has pointer focus
 * and that the pointer is inside the {@link ConfinedPointerV1Interface::region} when
 * it activates the confinement.
 *
 * From client side the confinement gets deactivated by destroying the ConfinedPointerV1Interface.
 * From compositor side the confinement can be deactivated by setting
 * {@link ConfinedPointerV1Interface::setConfined} to @c false.
 */
class KWIN_EXPORT ConfinedPointerV1Interface : public QObject
{
    Q_OBJECT

public:
    ~ConfinedPointerV1Interface() override;

    enum class LifeTime : uint {
        OneShot = 1,
        Persistent = 2,
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
     */
    QRegion region() const;

    /**
     * Whether the Compositor set this pointer confinement to be active.
     * @see setConfined
     * @see confinedChanged
     */
    bool isConfined() const;

    /**
     * Activates or deactivates the confinement.
     *
     * A pointer confinement can only be activated if the SurfaceInterface
     * this ConfinedPointerV1Interface was created for has pointer focus
     * and the pointer is inside the {@link region}.
     *
     * @param confined Whether the confinement should be active
     * @see isConfined
     * @see confinedChanged
     */
    void setConfined(bool confined);

Q_SIGNALS:
    /**
     * Emitted whenever the region changes.
     * This happens when the parent SurfaceInterface gets committed
     * @see region
     */
    void regionChanged();

    /**
     * Emitted whenever the {@link isConfined} state changes.
     * @see isConfined
     * @see setConfined
     */
    void confinedChanged();

private:
    ConfinedPointerV1Interface(SurfaceInterface *surface, LifeTime lifeTime, const QRegion &region, ::wl_resource *resource);
    std::unique_ptr<ConfinedPointerV1InterfacePrivate> d;
    friend class ConfinedPointerV1InterfacePrivate;
    friend class PointerConstraintsV1InterfacePrivate;
};

} // namespace KWaylandServer

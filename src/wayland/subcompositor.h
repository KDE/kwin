/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{
class Display;
class SubCompositorInterfacePrivate;
class SurfaceInterface;
class SurfaceRole;
class SubSurfaceInterface;
class SubSurfaceInterfacePrivate;

/**
 * The SubCompositorInterface compositor extension provides applications a way to offload
 * compositing work within a window from clients to the compositor. This may reduce the power
 * usage for applications such as video players, etc.
 *
 * The SubCompositorInterface corresponds to the Wayland interface @c wl_subcompositor.
 */
class KWIN_EXPORT SubCompositorInterface : public QObject
{
    Q_OBJECT

public:
    explicit SubCompositorInterface(Display *display, QObject *parent = nullptr);
    ~SubCompositorInterface() override;

Q_SIGNALS:
    /**
     * This signal is emitted when a new sub-surface @subsurface has been created.
     */
    void subSurfaceCreated(KWin::SubSurfaceInterface *subsurface);

private:
    std::unique_ptr<SubCompositorInterfacePrivate> d;
};

/**
 * The SubSurfaceInterface corresponds to the Wayland interface @c wl_subsurface.
 */
class KWIN_EXPORT SubSurfaceInterface : public QObject
{
    Q_OBJECT

public:
    ~SubSurfaceInterface() override;

    static SurfaceRole *role();

    /**
     * Returns the position of the sub-surface relative to the upper-left corner of its parent.
     */
    QPoint position() const;

    /**
     * This enum type is used to specify the commit behavior for a subsurface.
     */
    enum class Mode {
        Synchronized,
        Desynchronized,
    };

    /**
     * Returns the current commit mode.
     *
     * @see isSynchronized
     */
    Mode mode() const;

    /**
     * Returns @c true if the sub-surface is in synchronized mode; otherwise returns @c false.
     *
     * This method checks whether this sub-surface or any of its ancestors is in the synchronized
     * mode. Note that this function is not equivalent to calling mode() and checking whether
     * the return value is Mode::Synchronized.
     *
     * @see mode
     */
    bool isSynchronized() const;

    /**
     * Returns the SurfaceInterface for this SubSurfaceInterface. This function never returns a
     * @c null.
     */
    SurfaceInterface *surface() const;

    /**
     * Returns the parent surface for this SubSurfaceInterface. This function may return @c null.
     */
    SurfaceInterface *parentSurface() const;

    /**
     * Returns the main surface for the sub-surface tree, that is the first surface without a parent
     */
    SurfaceInterface *mainSurface() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the position of the sub-surface has changed.
     */
    void positionChanged(const QPoint &position);
    /**
     * This signal is emitted when the commit mode of the sub-surface has changed.
     */
    void modeChanged(KWin::SubSurfaceInterface::Mode mode);

private:
    SubSurfaceInterface(SurfaceInterface *surface, SurfaceInterface *parent, wl_resource *resource);

    void parentDesynchronized();
    void parentApplyState(quint32 serial);

    std::unique_ptr<SubSurfaceInterfacePrivate> d;
    friend class SurfaceInterfacePrivate;
    friend class SubSurfaceInterfacePrivate;
    friend class SubCompositorInterfacePrivate;
};

} // namespace KWin

Q_DECLARE_METATYPE(KWin::SubSurfaceInterface::Mode)

/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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
class SeatInterface;
class SurfaceInterface;
class SurfaceRole;
class XXPipV1Interface;
class XXPipV1InterfacePrivate;
class XdgSurfaceInterface;
class XXPipShellV1InterfacePrivate;

enum class Gravity;

/**
 * The XXPipShellV1Interface extension provides clients a way to create picture-in-picture
 * surfaces.
 */
class KWIN_EXPORT XXPipShellV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit XXPipShellV1Interface(Display *display, QObject *parent = nullptr);
    ~XXPipShellV1Interface() override;

    Display *display() const;

Q_SIGNALS:
    void pipCreated(XXPipV1Interface *pip);

private:
    std::unique_ptr<XXPipShellV1InterfacePrivate> d;
};

/**
 * The XXPipV1Interface class represents a picture-in-picture surface.
 *
 * XXPipV1Interface corresponds to the Wayland interface \c xx_pip_v1.
 */
class KWIN_EXPORT XXPipV1Interface : public QObject
{
    Q_OBJECT

public:
    XXPipV1Interface(XXPipShellV1Interface *shell, XdgSurfaceInterface *xdgSurface, wl_resource *resource);
    ~XXPipV1Interface() override;

    static SurfaceRole *role();

    /**
     * Returns \c true if the popup has been configured; otherwise returns \c false.
     */
    bool isConfigured() const;

    /**
     * Returns the XdgSurfaceInterface associated with the XXPipV1Interface.
     */
    XdgSurfaceInterface *xdgSurface() const;

    /**
     * Returns the SurfaceInterface associated with the XXPipV1Interface.
     */
    SurfaceInterface *surface() const;

    /**
     * Returns the desktop file name of the pip surface.
     */
    QString applicationId() const;

    /**
     * Returns the surface from which the picture-in-picture surface has been launched, or \c null.
     */
    SurfaceInterface *origin() const;

    /**
     * Specifies the bounds within the origin surface from which the picture-in-picture surface has
     * been launched.
     */
    QRect originRect() const;

    /**
     * Sends a configure event to the client. \a size specifies the new window geometry size. A size
     * of zero means the client should decide its own window dimensions.
     */
    quint32 sendConfigureSize(const QSizeF &size);

    /**
     * Sends a close event to the client. The client may choose to ignore this request.
     */
    void sendClosed();

    /**
     * Sends an event to the client specifying the maximum bounds for the surface size. Must be
     * called before sendConfigure().
     */
    void sendConfigureBounds(const QSizeF &size);

    /**
     * Returns the XXPipV1Interface for the specified wayland resource object \a resource.
     */
    static XXPipV1Interface *get(::wl_resource *resource);

Q_SIGNALS:
    /**
     * This signal is emitted when the xx-pip-v1 is about to be destroyed.
     */
    void aboutToBeDestroyed();

    /**
     * This signal is emitted when the xx-pip-v1 has commited the initial state and wants to
     * be configured. After initializing the pip surface, you must send a configure event.
     */
    void initializeRequested();

    /**
     * This signal is emitted when the pip surface has been unmapped and its state has been reset.
     */
    void resetOccurred();

    /**
     * This signal is emitted when the pip wants to be interactively moved. The \a seat and
     * the \a serial indicate the user action in response to which this request has been issued.
     */
    void moveRequested(SeatInterface *seat, quint32 serial);

    /**
     * This signal is emitted when the pip wants to be interactively resized with
     * the specified \a gravity. The \a seat and the \a serial indicate the user action
     * in response to which this request has been issued.
     */
    void resizeRequested(SeatInterface *seat, Gravity anchor, quint32 serial);

    /**
     * This signal is emitted when the application id changes.
     */
    void applicationIdChanged();

private:
    std::unique_ptr<XXPipV1InterfacePrivate> d;
};

} // namespace KWin

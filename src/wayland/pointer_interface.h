/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class CursorPrivate;
class Cursor;
class PointerInterfacePrivate;
class SeatInterface;
class SurfaceInterface;

enum class PointerAxisSource;
enum class PointerButtonState : quint32;

/**
 * The PointerInterface class represents one or more input devices such as mice, which control
 * the pointer location. It corresponds to the Wayland interface @c wl_pointer.
 */
class KWIN_EXPORT PointerInterface : public QObject
{
    Q_OBJECT

public:
    ~PointerInterface() override;

    /**
     * Returns the focused pointer surface. Note that the returned value may be different
     * from SurfaceInterface::focusedSurfacePointerSurface() because this function returns
     * the effective focused surface.
     */
    SurfaceInterface *focusedSurface() const;

    Cursor *cursor() const;

    /**
     * Returns the seat to which this pointer belongs to.
     */
    SeatInterface *seat() const;

    /**
     * @returns The PointerInterface for the @p native resource.
     */
    static PointerInterface *get(wl_resource *native);

    void sendEnter(SurfaceInterface *surface, const QPointF &position, quint32 serial);
    void sendLeave(quint32 serial);
    void sendButton(quint32 button, PointerButtonState state, quint32 serial);
    void sendAxis(Qt::Orientation orientation, qreal delta, qint32 deltaV120, PointerAxisSource source);
    void sendMotion(const QPointF &position);
    void sendFrame();

Q_SIGNALS:
    /**
     * This signal is emitted whenever the cursor surface changes. As long as there is no
     * any focused surface, the cursor cannot be changed.
     */
    void cursorChanged();
    /**
     * This signal is emitted whenever the focused pointer surface changes.
     */
    void focusedSurfaceChanged();

private:
    explicit PointerInterface(SeatInterface *seat);
    std::unique_ptr<PointerInterfacePrivate> d;

    friend class SeatInterfacePrivate;
    friend class PointerInterfacePrivate;
};

/**
 * @brief Class encapsulating a Cursor image.
 */
class KWIN_EXPORT Cursor : public QObject
{
    Q_OBJECT

public:
    virtual ~Cursor();
    /**
     * The hotspot of the cursor image in surface-relative coordinates.
     */
    QPoint hotspot() const;
    /**
     * The entered serial when the Cursor got set.
     */
    quint32 enteredSerial() const;
    /**
     * The PointerInterface this Cursor belongs to.
     */
    PointerInterface *pointer() const;
    /**
     * The SurfaceInterface for the image content of the Cursor.
     */
    SurfaceInterface *surface() const;

Q_SIGNALS:
    void hotspotChanged();
    void enteredSerialChanged();
    void surfaceChanged();
    void changed();

private:
    std::unique_ptr<CursorPrivate> d;
    friend class PointerInterfacePrivate;
    explicit Cursor(PointerInterface *parent);
};

} // namespace KWaylandServer

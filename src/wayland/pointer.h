/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/inputdevice.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{
class PointerSurfaceCursorPrivate;
class PointerSurfaceCursor;
class PointerInterfacePrivate;
class ClientConnection;
class SeatInterface;
class SurfaceInterface;

using PointerCursor = std::variant<PointerSurfaceCursor *, QByteArray>;

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
    quint32 focusedSerial() const;

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
    void sendButton(quint32 button, PointerButtonState state, quint32 serial, ClientConnection *client);
    void sendAxis(Qt::Orientation orientation, qreal delta, qint32 deltaV120, PointerAxisSource source, bool inverted);
    void sendMotion(const QPointF &position);
    void sendFrame();

Q_SIGNALS:
    /**
     * This signal is emitted whenever the cursor surface changes. As long as there is no
     * any focused surface, the cursor cannot be changed.
     */
    void cursorChanged(const PointerCursor &cursor);
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
class KWIN_EXPORT PointerSurfaceCursor
{
public:
    PointerSurfaceCursor();
    ~PointerSurfaceCursor();

    /**
     * The hotspot of the cursor image in surface-relative coordinates.
     */
    QPointF hotspot() const;
    /**
     * The SurfaceInterface for the image content of the Cursor.
     */
    SurfaceInterface *surface() const;

private:
    std::unique_ptr<PointerSurfaceCursorPrivate> d;
    friend class PointerInterfacePrivate;
};

} // namespace KWin

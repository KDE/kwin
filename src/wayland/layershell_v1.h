/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "display.h"

#include <QMargins>

namespace KWin
{
class LayerShellV1InterfacePrivate;
class LayerSurfaceV1Interface;
class LayerSurfaceV1InterfacePrivate;
class OutputInterface;
class SurfaceInterface;
class SurfaceRole;

/**
 * The LayerShellV1Interface compositor extension allows to create desktop shell surfaces.
 *
 * The layer shell compositor extension provides a way to create surfaces that can serve as
 * building blocks for desktop environment elements such as panels, notifications, etc.
 *
 * The LayerShellV1Interface corresponds to the Wayland interface @c zwlr_layer_shell_v1.
 */
class KWIN_EXPORT LayerShellV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit LayerShellV1Interface(Display *display, QObject *parent = nullptr);
    ~LayerShellV1Interface() override;

    /**
     * Returns the Wayland display for the layer shell compositor extension.
     */
    Display *display() const;

Q_SIGNALS:
    /**
     * This signal is emitted when a new layer surface @a surface has been created.
     */
    void surfaceCreated(LayerSurfaceV1Interface *surface);

private:
    std::unique_ptr<LayerShellV1InterfacePrivate> d;
};

/**
 * The LayerSurfaceV1Interface class represents a desktop shell surface, e.g. panel, etc.
 *
 * The LayerSurfaceV1Interface corresponds to the Wayland interface @c zwlr_layer_surface_v1.
 */
class KWIN_EXPORT LayerSurfaceV1Interface : public QObject
{
    Q_OBJECT

public:
    enum Layer {
        BackgroundLayer,
        BottomLayer,
        TopLayer,
        OverlayLayer,
    };

    LayerSurfaceV1Interface(LayerShellV1Interface *shell,
                            SurfaceInterface *surface,
                            OutputInterface *output,
                            Layer layer,
                            const QString &scope,
                            wl_resource *resource);
    ~LayerSurfaceV1Interface() override;

    static SurfaceRole *role();

    /**
     * Returns @c true if the initial commit has been performed; otherwise returns @c false.
     */
    bool isCommitted() const;

    /**
     * Returns the underlying Wayland surface for this layer shell surface.
     */
    SurfaceInterface *surface() const;

    /**
     * Returns the anchor point relative to which the surface will be positioned. If no edges
     * have been specified, the center of the screen is assumed to be the anchor point.
     */
    Qt::Edges anchor() const;

    /**
     * Returns the desired size for this layer shell surface, in the surface-local coordinates.
     */
    QSize desiredSize() const;

    /**
     * Returns the stacking order layer where this layer surface has to be rendered.
     */
    Layer layer() const;

    /**
     * Returns @c true if the surface accepts keyboard input; otherwise returns @c false.
     */
    bool acceptsFocus() const;

    /**
     * Returns the margins object that indicates the distance between an anchor edge and
     * the corresponding surface edge.
     */
    QMargins margins() const;

    /**
     * Returns the value of the left margin. This is equivalent to calling margins().left().
     */
    int leftMargin() const;

    /**
     * Returns the value of the right margin. This is equivalent to calling margins().right().
     */
    int rightMargin() const;

    /**
     * Returns the value of the top margin. This is equivalent to calling margins().top().
     */
    int topMargin() const;

    /**
     * Returns the value of the bottom margin. This is equivalent to calling margins().bottom().
     */
    int bottomMargin() const;

    /**
     * Returns the distance from the anchor edge that should not be occluded.
     *
     * An exlusive zone of 0 means that the layer surface has to be moved to avoid occluding
     * surfaces with a positive exclusion zone. If the exclusive zone is -1, the layer surface
     * indicates that it doesn't want to be moved to accomodate for other surfaces.
     */
    int exclusiveZone() const;

    /**
     * If the exclusive zone is positive, this function returns the corresponding exclusive
     * anchor edge, otherwise returns a Qt::Edge() value.
     */
    Qt::Edge exclusiveEdge() const;

    /**
     * Returns the output where the surface wants to be displayed. This function can return
     * @c null, in which case the compositor is free to choose the output where the surface
     * has to be placed.
     */
    OutputInterface *output() const;

    /**
     * Returns the scope of this layer surface. The scope defines the purpose of the surface.
     */
    QString scope() const;

    /**
     * Sends a configure event to the client. @a size contains the desired size in surface-local
     * coordinates. A size of zero means that the client is free to choose its own dimensions.
     *
     * @see configureAcknowledged()
     */
    quint32 sendConfigure(const QSize &size);

    /**
     * Sends a closed event to the client. The client should destroy the surface after receiving
     * this event. Further changes to the surface will be ignored.
     */
    void sendClosed();

Q_SIGNALS:
    void aboutToBeDestroyed();
    void configureAcknowledged(quint32 serial);
    void acceptsFocusChanged();
    void layerChanged();
    void anchorChanged();
    void desiredSizeChanged();
    void exclusiveZoneChanged();
    void marginsChanged();

private:
    std::unique_ptr<LayerSurfaceV1InterfacePrivate> d;
};

} // namespace KWin

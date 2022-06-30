/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/output.h"
#include "output_interface.h"

#include <QMatrix4x4>
#include <QObject>
#include <QPointer>
#include <QRegion>

namespace KWaylandServer
{
class BlurInterface;
class ClientBuffer;
class ConfinedPointerV1Interface;
class ContrastInterface;
class CompositorInterface;
class LockedPointerV1Interface;
class ShadowInterface;
class SlideInterface;
class SubSurfaceInterface;
class SurfaceInterfacePrivate;
class LinuxDmaBufV1Feedback;

enum class PresentationHint {
    VSync,
    Async
};

/**
 * @brief Resource representing a wl_surface.
 *
 * The SurfaceInterface gets created by the CompositorInterface. A SurfaceInterface normally
 * takes up a role by being "attached" to either a ShellSurfaceInterface, a SubSurfaceInterface
 * or a Cursor.
 *
 * The implementation of the SurfaceInterface does not only wrap the features exposed by wl_surface,
 * but goes further by integrating the information added to a SurfaceInterface by other interfaces.
 * This should make interacting from the server easier, it only needs to monitor the SurfaceInterface
 * and does not need to track each specific interface.
 *
 * The SurfaceInterface takes care of reference/unreferencing the ClientBuffer attached to it.
 * As long as a ClientBuffer is attached, the released signal won't be sent. If the ClientBuffer
 * is no longer needed by the SurfaceInterface, it will get unreferenced and might be automatically
 * deleted (if it's no longer referenced).
 *
 * @see CompositorInterface
 * @see ClientBuffer
 * @see SubSurfaceInterface
 * @see BlurInterface
 * @see ContrastInterface
 * @see ShadowInterface
 * @see SlideInterface
 * @see LinuxDmaBufV1Feedback
 */
class KWIN_EXPORT SurfaceInterface : public QObject
{
    Q_OBJECT
    /**
     * The current damage region.
     */
    Q_PROPERTY(QRegion damage READ damage NOTIFY damaged)
    /**
     * The opaque region for a translucent buffer.
     */
    Q_PROPERTY(QRegion opaque READ opaque NOTIFY opaqueChanged)
    /**
     * The current input region.
     */
    Q_PROPERTY(QRegion input READ input NOTIFY inputChanged)
    Q_PROPERTY(qint32 bufferScale READ bufferScale NOTIFY bufferScaleChanged)
    Q_PROPERTY(KWin::Output::Transform bufferTransform READ bufferTransform NOTIFY bufferTransformChanged)
    Q_PROPERTY(QSizeF size READ size NOTIFY sizeChanged)
public:
    explicit SurfaceInterface(CompositorInterface *compositor, wl_resource *resource);
    ~SurfaceInterface() override;

    /**
     * Returns the object id for this Wayland surface.
     */
    uint32_t id() const;
    /**
     * Returns the Wayland client that owns this SurfaceInterface.
     */
    ClientConnection *client() const;
    /**
     * Returns the Wayland resource corresponding to this SurfaceInterface.
     */
    wl_resource *resource() const;
    /**
     * Returns the compositor for this SurfaceInterface.
     */
    CompositorInterface *compositor() const;

    /**
     * Maps the specified @a point from the surface-local coordinates to buffer pixel coordinates.
     *
     * Note that there is no direct connection between points in the surface-local coordinates
     * and points in the buffer pixel coordinates. In order to map points between the two spaces,
     * one has to use mapToBuffer() and mapFromBuffer().
     *
     * The returned value will become invalid when the surfaceToBufferMatrixChanged() signal is emitted.
     *
     * @see surfaceToBufferMatrix(), surfaceToBufferMatrixChanged()
     */
    QPointF mapToBuffer(const QPointF &point) const;
    /**
     * Maps the specified @a point from the buffer pixel coordinates to surface-local coordinates.
     *
     * Note that there is no direct connection between points in the surface-local coordinates
     * and points in the buffer pixel coordinates. In order to map points between the two spaces,
     * one has to use mapToBuffer() and mapFromBuffer().
     *
     * The returned value will become invalid when the surfaceToBufferMatrixChanged() signal is emitted.
     *
     * @see surfaceToBufferMatrix(), surfaceToBufferMatrixChanged()
     */
    QPointF mapFromBuffer(const QPointF &point) const;
    /**
     * Maps the specified @a region from the surface-local coordinates to buffer pixel coordinates.
     *
     * Note that there is no direct connection between regions in the surface-local coordinates
     * and regions in the buffer pixel coordinates. In order to map regions between the two spaces,
     * one has to use mapToBuffer() and mapFromBuffer().
     *
     * The returned value will become invalid when the surfaceToBufferMatrixChanged() signal is emitted.
     *
     * @see surfaceToBufferMatrix(), surfaceToBufferMatrixChanged()
     */
    QRegion mapToBuffer(const QRegion &region) const;
    /**
     * Maps the specified @a region from the buffer pixel coordinates to surface-local coordinates.
     *
     * Note that there is no direct connection between regions in the surface-local coordinates
     * and regions in the buffer pixel coordinates. In order to map regions between the two spaces,
     * one has to use mapToBuffer() and mapFromBuffer().
     *
     * The returned value will become invalid when the surfaceToBufferMatrixChanged() signal is emitted.
     *
     * @see surfaceToBufferMatrix(), surfaceToBufferMatrixChanged()
     */
    QRegion mapFromBuffer(const QRegion &region) const;
    /**
     * Returns the projection matrix from the surface-local coordinates to buffer coordinates.
     *
     * @see surfaceToBufferMatrixChanged()
     */
    QMatrix4x4 surfaceToBufferMatrix() const;

    /**
     * Maps the specified @a point in this surface's coordinate system to the equivalent point
     * within the @a child's coordinate system, and returns the mapped point.
     *
     * If this surface is not an ancestor of the @a child, a null point is returned.
     */
    QPointF mapToChild(SurfaceInterface *child, const QPointF &point) const;

    void frameRendered(quint32 msec);
    bool hasFrameCallbacks() const;

    QRegion damage() const;
    QRegion opaque() const;
    QRegion input() const;
    qint32 bufferScale() const;
    /**
     * Returns the buffer transform that had been applied to the buffer to compensate for
     * output rotation.
     *
     * If the surface is on an output that is rotated 90 degrees clockwise, the buffer will
     * be rotated 90 degrees counter clockwise.
     */
    KWin::Output::Transform bufferTransform() const;
    /**
     * @returns the current ClientBuffer, might be @c nullptr.
     */
    ClientBuffer *buffer() const;
    QPoint offset() const;
    /**
     * Returns the current size of the surface, in surface coordinates.
     *
     * Note that there is no direct relationship between the surface size and the buffer size.
     * In order to determine the size of the currently attached buffer, use buffer()->size().
     */
    QSizeF size() const;
    /**
     * Returns the rectangle that bounds this surface and all of its sub-surfaces.
     *
     * QPoint(0, 0) corresponds to the upper left corner of this surface.
     */
    QRectF boundingRect() const;
    /**
     * Returns the size of the attached buffer, in device pixels.
     *
     * If no buffer is attached to this surface, an invalid QSize will be returned.
     */
    QSize bufferSize() const;

    /**
     * @returns The SubSurface for this Surface in case there is one.
     */
    SubSurfaceInterface *subSurface() const;
    /**
     * Returns the sub-surfaces that are below this surface. The sub-surfaces are sorted
     * from bottom to top.
     */
    QList<SubSurfaceInterface *> below() const;
    /**
     * Returns the sub-surfaces that are above this surface. The sub-surfaces are sorted
     * from bottom to top.
     */
    QList<SubSurfaceInterface *> above() const;

    /**
     * @returns The Shadow for this Surface.
     */
    QPointer<ShadowInterface> shadow() const;

    /**
     * @returns The Blur for this Surface.
     */
    QPointer<BlurInterface> blur() const;

    /**
     * @returns The Slide for this Surface.
     */
    QPointer<SlideInterface> slideOnShowHide() const;

    /**
     * @returns The Contrast for this Surface.
     */
    QPointer<ContrastInterface> contrast() const;

    /**
     * Whether the SurfaceInterface is currently considered to be mapped.
     * A SurfaceInterface is mapped if it has a non-null ClientBuffer attached.
     * If the SurfaceInterface references a SubSurfaceInterface it is only considered
     * mapped if it has a ClientBuffer attached and the parent SurfaceInterface is mapped.
     *
     * @returns Whether the SurfaceInterface is currently mapped
     */
    bool isMapped() const;

    /**
     * Finds the SurfaceInterface at the given @p position in surface-local coordinates.
     * This can be either a descendant SurfaceInterface honoring the stacking order or
     * the SurfaceInterface itself if its geometry contains the given @p position.
     *
     * If no such SurfaceInterface is found, e.g. because the SurfaceInterface is unmapped,
     * @c nullptr is returned.
     *
     * @param position The position in surface-local coordinates
     * @returns Child surface at the given @p position or surface itself at the position, might be @c nullptr
     */
    SurfaceInterface *surfaceAt(const QPointF &position);

    /**
     * Finds the input receiving SurfaceInterface at the given @p position in surface-local coordinates.
     * This can be either a descendant SurfaceInterface honoring the stacking order or
     * the SurfaceInterface itself if its geometry contains the given @p position.
     *
     * If no such SurfaceInterface is found, e.g. because the SurfaceInterface is unmapped or there is no
     * input region containing the position,
     * @c nullptr is returned.
     *
     * @param position The position in surface-local coordinates
     * @returns Input receiving child surface at the given @p position or surface itself at the position, might be @c nullptr
     */
    SurfaceInterface *inputSurfaceAt(const QPointF &position);

    /**
     * Sets the @p outputs this SurfaceInterface overlaps with, may be empty.
     *
     * The compositor should update whenever the SurfaceInterface becomes visible on
     * an OutputInterface by e.g. getting (un)mapped, resized, moved, etc.
     *
     * @see outputs
     */
    void setOutputs(const QVector<OutputInterface *> &outputs);

    /**
     * @returns All OutputInterfaces the SurfaceInterface is on.
     * @see setOutputs
     */
    QVector<OutputInterface *> outputs() const;

    /**
     * Pointer confinement installed on this SurfaceInterface.
     * @see pointerConstraintsChanged
     */
    ConfinedPointerV1Interface *confinedPointer() const;

    /**
     * Pointer lock installed on this SurfaceInterface.
     * @see pointerConstraintsChanged
     */
    LockedPointerV1Interface *lockedPointer() const;

    /**
     * @returns Whether this SurfaceInterface wants idle to be inhibited on the Output it is shown
     * @see inhibitsIdleChanged
     */
    bool inhibitsIdle() const;

    /**
     * dmabuf feedback installed on this SurfaceInterface
     */
    LinuxDmaBufV1Feedback *dmabufFeedbackV1() const;

    /**
     * @returns the current content type of this surface
     */
    KWin::ContentType contentType() const;

    /**
     * @returns The SurfaceInterface for the @p native resource.
     */
    static SurfaceInterface *get(wl_resource *native);
    /**
     * @returns The SurfaceInterface with given @p id for @p client, if it exists, otherwise @c nullptr.
     */
    static SurfaceInterface *get(quint32 id, const ClientConnection *client);

    /**
     * @see ClientConnection::setScaleOverride
     */
    qreal scaleOverride() const;
    /**
     * Convert a co-ordinate from kwin logical space to surface logical space
     * @internal
     */
    QPoint toSurfaceLocal(const QPoint &point) const;
    /**
     * Convert a co-ordinate from kwin logical space to surface logical space
     * @internal
     */
    QPointF toSurfaceLocal(const QPointF &point) const;

    /**
     * @returns if the client thinks the content of this surface is suitable for presentation with tearing
     */
    PresentationHint presentationHint() const;

    /**
    * Sets a preferred scale that clients should provide buffers in
     * @param scale
     */
    void setPreferredScale(qreal scale);

Q_SIGNALS:
    /**
     * This signal is emitted when the underlying wl_surface resource is about to be freed.
     *
     * The unbound() signal is emitted either when the client that owns the surface has been
     * destroyed or if the surface has been destroyed due to a destructor request.
     *
     * The SurfaceInterface object and the associated wl_surface resource are valid when this
     * signal is emitted.
     */
    void aboutToBeDestroyed();
    /**
     * This signal is emitted when the projection matrix from the surface-local coordinate space
     * to the buffer coordinate space has been changed.
     *
     * Note that the compositor will most likely need to re-compute the texture coordinates after
     * the surface-to-buffer matrix has been changed.
     */
    void surfaceToBufferMatrixChanged();
    /**
     * Emitted whenever the SurfaceInterface got damaged.
     * The signal is only emitted during the commit of state.
     * A damage means that a new ClientBuffer got attached.
     *
     * @see buffer
     * @see damage
     */
    void damaged(const QRegion &);
    void opaqueChanged(const QRegion &);
    void inputChanged(const QRegion &);
    /**
     * This signal is emitted when the scale of the attached buffer has changed.
     */
    void bufferScaleChanged(qint32);
    /**
     * This signal is emitted when the buffer transform has changed.
     */
    void bufferTransformChanged(KWin::Output::Transform);
    /**
     * This signal is emitted when the size of the attached buffer has changed.
     */
    void bufferSizeChanged();
    /**
     * Emitted when the Surface becomes visible, i.e. a non-null buffer has been attached.
     */
    void mapped();
    /**
     * Emitted when the Surface removes its content
     */
    void unmapped();
    /**
     * This signal is emitted when the surface size has changed.
     */
    void sizeChanged();
    void shadowChanged();
    void blurChanged();
    void slideOnShowHideChanged();
    void contrastChanged();
    /**
     * Emitted whenever a new child sub-surface @p subSurface is added.
     */
    void childSubSurfaceAdded(SubSurfaceInterface *subSurface);
    /**
     * Emitted whenver the child sub-surface @p subSurface is removed.
     */
    void childSubSurfaceRemoved(SubSurfaceInterface *subSurface);
    /**
     * This signal is emitted when the list of child subsurfaces changes.
     */
    void childSubSurfacesChanged();

    /**
     * Emitted whenever a pointer constraint get (un)installed on this SurfaceInterface.
     *
     * The pointer constraint does not get activated, the compositor needs to activate
     * the lock/confinement.
     *
     * @see confinedPointer
     * @see lockedPointer
     */
    void pointerConstraintsChanged();

    /**
     * Emitted whenever the SurfaceInterface starts/ends to inhibit idle.
     * @see inhibitsIdle
     */
    void inhibitsIdleChanged();

    /**
     * Emitted when the Surface has been committed.
     *
     * This signal is emitted after all the relevant damage and xyzChanged signals
     * for this commit are emitted.
     */
    void committed();

private:
    std::unique_ptr<SurfaceInterfacePrivate> d;
    friend class SurfaceInterfacePrivate;
};

}

Q_DECLARE_METATYPE(KWaylandServer::SurfaceInterface *)

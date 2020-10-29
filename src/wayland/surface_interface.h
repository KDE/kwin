/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_SURFACE_INTERFACE_H
#define WAYLAND_SERVER_SURFACE_INTERFACE_H

#include "output_interface.h"

#include <QMatrix4x4>
#include <QObject>
#include <QPointer>
#include <QRegion>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{
class BlurInterface;
class BufferInterface;
class ConfinedPointerInterface;
class ContrastInterface;
class CompositorInterface;
class LockedPointerInterface;
class ShadowInterface;
class SlideInterface;
class SubSurfaceInterface;
class SurfaceInterfacePrivate;

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
 * The SurfaceInterface takes care of reference/unreferencing the BufferInterface attached to it.
 * As long as a BufferInterface is attached, the released signal won't be sent. If the BufferInterface
 * is no longer needed by the SurfaceInterface, it will get unreferenced and might be automatically
 * deleted (if it's no longer referenced).
 *
 * @see CompositorInterface
 * @see BufferInterface
 * @see SubSurfaceInterface
 * @see BlurInterface
 * @see ContrastInterface
 * @see ShadowInterface
 * @see SlideInterface
 **/
class KWAYLANDSERVER_EXPORT SurfaceInterface : public QObject
{
    Q_OBJECT
    /**
     * The current damage region.
     **/
    Q_PROPERTY(QRegion damage READ damage NOTIFY damaged)
    /**
     * The opaque region for a translucent buffer.
     **/
    Q_PROPERTY(QRegion opaque READ opaque NOTIFY opaqueChanged)
    /**
     * The current input region.
     **/
    Q_PROPERTY(QRegion input READ input NOTIFY inputChanged)
    Q_PROPERTY(qint32 bufferScale READ bufferScale NOTIFY bufferScaleChanged)
    Q_PROPERTY(KWaylandServer::OutputInterface::Transform bufferTransform READ bufferTransform NOTIFY bufferTransformChanged)
    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
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
     * Returns a list with all created Wayland surfaces.
     */
    static QList<SurfaceInterface *> surfaces();

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
     * @since 5.20
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
     * @since 5.20
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
     * @since 5.20
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
     * @since 5.20
     */
    QRegion mapFromBuffer(const QRegion &region) const;
    /**
     * Returns the projection matrix from the surface-local coordinates to buffer coordinates.
     *
     * @see surfaceToBufferMatrixChanged()
     * @since 5.20
     */
    QMatrix4x4 surfaceToBufferMatrix() const;

    void frameRendered(quint32 msec);

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
    OutputInterface::Transform bufferTransform() const;
    /**
     * @returns the current BufferInterface, might be @c nullptr.
     **/
    BufferInterface *buffer();
    QPoint offset() const;
    /**
     * Returns the current size of the surface, in surface coordinates.
     *
     * Note that there is no direct relationship between the surface size and the buffer size.
     * In order to determine the size of the currently attached buffer, use buffer()->size().
     */
    QSize size() const;
    /**
     * Returns the rectangle that bounds this surface and all of its sub-surfaces.
     *
     * QPoint(0, 0) corresponds to the upper left corner of this surface.
     *
     * @since 5.69
     */
    QRect boundingRect() const;
    /**
     * Returns the size of the attached buffer, in device pixels.
     *
     * If no buffer is attached to this surface, an invalid QSize will be returned.
     */
    QSize bufferSize() const;

    /**
     * @returns The SubSurface for this Surface in case there is one.
     **/
    QPointer<SubSurfaceInterface> subSurface() const;
    /**
     * @returns Children in stacking order from bottom (first) to top (last).
     **/
    QList<QPointer<SubSurfaceInterface>> childSubSurfaces() const;

    /**
     * @returns The Shadow for this Surface.
     * @since 5.4
     **/
    QPointer<ShadowInterface> shadow() const;

    /**
     * @returns The Blur for this Surface.
     * @since 5.5
     **/
    QPointer<BlurInterface> blur() const;

    /**
     * @returns The Slide for this Surface.
     * @since 5.5
     **/
    QPointer<SlideInterface> slideOnShowHide() const;

    /**
     * @returns The Contrast for this Surface.
     * @since 5.5
     **/
    QPointer<ContrastInterface> contrast() const;

    /**
     * Whether the SurfaceInterface is currently considered to be mapped.
     * A SurfaceInterface is mapped if it has a non-null BufferInterface attached.
     * If the SurfaceInterface references a SubSurfaceInterface it is only considered
     * mapped if it has a BufferInterface attached and the parent SurfaceInterface is mapped.
     *
     * @returns Whether the SurfaceInterface is currently mapped
     * @since 5.22
     **/
    bool isMapped() const;

    /**
     * Returns the tracked damage since the last call to  {@link resetTrackedDamage}.
     * In contrast to {@link damage} this method does not reset the damage when
     * a new BufferInterface gets committed. This allows a compositor to properly
     * track the damage over multiple commits even if it didn't render each new
     * BufferInterface.
     *
     * The damage gets reset whenever {@link resetTrackedDamage} is called.
     * This allows a compositor to properly track the change in its rendering scene
     * for this SurfaceInterface. After it updates its internal state (e.g. by creating
     * an OpenGL texture from the BufferInterface) it can invoke {@link resetTrackedDamage}
     * and the damage tracker will start to track further damage changes.
     *
     * @returns Combined damage since last call to resetTrackedDamage
     * @see damage
     * @see resetTrackedDamage
     * @since 5.22
     **/
    QRegion trackedDamage() const;

    /**
     * Reset the damage tracking. The compositor should invoke this method once it updated
     * it's internal state and processed the current damage.
     * @see trackedDamage
     * @since 5.22
     **/
    void resetTrackedDamage();

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
     * @since 5.22
     **/
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
     * @since 5.52
     **/
    SurfaceInterface *inputSurfaceAt(const QPointF &position);

    /**
     * Sets the @p outputs this SurfaceInterface overlaps with, may be empty.
     *
     * The compositor should update whenever the SurfaceInterface becomes visible on
     * an OutputInterface by e.g. getting (un)mapped, resized, moved, etc.
     *
     * @see outputs
     * @since 5.27
     **/
    void setOutputs(const QVector<OutputInterface *> &outputs);

    /**
     * @returns All OutputInterfaces the SurfaceInterface is on.
     * @see setOutputs
     * @since 5.27
     **/
    QVector<OutputInterface *> outputs() const;

    /**
     * Pointer confinement installed on this SurfaceInterface.
     * @see pointerConstraintsChanged
     * @since 5.29
     **/
    QPointer<ConfinedPointerInterface> confinedPointer() const;

    /**
     * Pointer lock installed on this SurfaceInterface.
     * @see pointerConstraintsChanged
     * @since 5.29
     **/
    QPointer<LockedPointerInterface> lockedPointer() const;

    /**
     * @returns Whether this SurfaceInterface wants idle to be inhibited on the Output it is shown
     * @see inhibitsIdleChanged
     * @since 5.41
     **/
    bool inhibitsIdle() const;

    /**
     * @returns The SurfaceInterface for the @p native resource.
     **/
    static SurfaceInterface *get(wl_resource *native);
    /**
     * @returns The SurfaceInterface with given @p id for @p client, if it exists, otherwise @c nullptr.
     * @since 5.3
     **/
    static SurfaceInterface *get(quint32 id, const ClientConnection *client);

    /**
     * Set @p surface as a data proxy for this SurfaceInterface. This enables
     * the proxy to conduct drags on the surface's client behalf.
     *
     * Setting a data proxy is only allowed when the client owning this surface
     * has not created a data device itself.
     * @since 5.56
     **/
    void setDataProxy(SurfaceInterface *surface);
    /**
     * Returns the data proxy of this SurfaceInterface or null if there
     * is none set.
     * @since 5.56
     **/
    SurfaceInterface* dataProxy() const;

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
     * A damage means that a new BufferInterface got attached.
     *
     * @see buffer
     * @see damage
     **/
    void damaged(const QRegion&);
    void opaqueChanged(const QRegion&);
    void inputChanged(const QRegion&);
    /**
     * This signal is emitted when the scale of the attached buffer has changed.
     */
    void bufferScaleChanged(qint32);
    /**
     * This signal is emitted when the buffer transform has changed.
     */
    void bufferTransformChanged(KWaylandServer::OutputInterface::Transform);
    /**
     * This signal is emitted when the size of the attached buffer has changed.
     */
    void bufferSizeChanged();
    /**
     * Emitted when the Surface becomes visible, i.e. a non-null buffer has been attached.
     **/
    void mapped();
    /**
     * Emitted when the Surface removes its content
     **/
    void unmapped();
    /**
     * This signal is emitted when the surface size has changed.
     */
    void sizeChanged();
    /**
     * @since 5.4
     **/
    void shadowChanged();
    /**
     * @since 5.5
     **/
    void blurChanged();
    /**
     * @since 5.5
     **/
    void slideOnShowHideChanged();
    /**
     * @since 5.5
     **/
    void contrastChanged();
    /**
     * Emitted whenever the tree of sub-surfaces changes in a way which requires a repaint.
     * @since 5.22
     **/
    void subSurfaceTreeChanged();
    /**
     * Emitted whenever a new child sub-surface @p subSurface is added.
     * @since 5.70
     */
    void childSubSurfaceAdded(SubSurfaceInterface *subSurface);
    /**
     * Emitted whenver the child sub-surface @p subSurface is removed.
     * @since 5.70
     */
    void childSubSurfaceRemoved(SubSurfaceInterface *subSurface);

    /**
     * Emitted whenever a pointer constraint get (un)installed on this SurfaceInterface.
     *
     * The pointer constraint does not get activated, the compositor needs to activate
     * the lock/confinement.
     *
     * @see confinedPointer
     * @see lockedPointer
     * @since 5.29
     **/
    void pointerConstraintsChanged();

    /**
     * Emitted whenever the SurfaceInterface starts/ends to inhibit idle.
     * @see inhibitsIdle
     * @since 5.41
     **/
    void inhibitsIdleChanged();

    /**
     * Emitted when the Surface has been committed.
     *
     * This signal is emitted after all the relevant damage and xyzChanged signals
     * for this commit are emitted.
     * @since 5.54
     **/
    void committed();

private:
    void handleBufferRemoved(BufferInterface *buffer);

    QScopedPointer<SurfaceInterfacePrivate> d;
    friend class SurfaceInterfacePrivate;
};

}

Q_DECLARE_METATYPE(KWaylandServer::SurfaceInterface*)

#endif

/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/colorspace.h"
#include "core/output.h"
#include "core/renderbackend.h"

#include <QObject>
#include <QRegion>

struct wl_resource;

namespace KWin
{

class GraphicsBuffer;
class BlurInterface;
class ClientConnection;
class ConfinedPointerV1Interface;
class ContrastInterface;
class CompositorInterface;
class LinuxDmaBufV1Feedback;
class LockedPointerV1Interface;
class OutputInterface;
class ShadowInterface;
class SlideInterface;
class SubSurfaceInterface;
class SurfaceInterfacePrivate;
class Transaction;
class SyncReleasePoint;

/**
 * The SurfaceRole class represents a role assigned to a wayland surface.
 */
class KWIN_EXPORT SurfaceRole
{
public:
    explicit SurfaceRole(const QByteArray &name);

    /**
     * The human readable name of the surface role.
     */
    QByteArray name() const;

private:
    QByteArray m_name;
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
 * The SurfaceInterface takes care of reference/unreferencing the GraphicsBuffer attached to it.
 * As long as a GraphicsBuffer is attached, the released signal won't be sent. If the GraphicsBuffer
 * is no longer needed by the SurfaceInterface, it will get unreferenced and might be automatically
 * deleted (if it's no longer referenced).
 *
 * @see CompositorInterface
 * @see GraphicsBuffer
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
     * The opaque region for a translucent buffer.
     */
    Q_PROPERTY(QRegion opaque READ opaque NOTIFY opaqueChanged)
    /**
     * The current input region.
     */
    Q_PROPERTY(QRegion input READ input NOTIFY inputChanged)
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
     * Returns the role of this surface, or @c null if no role has been assigned to the surface.
     *
     * Once a role is given to the surface, it is permanent.
     */
    SurfaceRole *role() const;
    void setRole(SurfaceRole *role);

    /**
     * Maps the specified @a point in this surface's coordinate system to the equivalent point
     * within the @a child's coordinate system, and returns the mapped point.
     *
     * If this surface is not an ancestor of the @a child, a null point is returned.
     */
    QPointF mapToChild(SurfaceInterface *child, const QPointF &point) const;

    void frameRendered(quint32 msec);
    bool hasFrameCallbacks() const;

    std::unique_ptr<PresentationFeedback> takePresentationFeedback(Output *output);

    QRegion opaque() const;
    QRegion input() const;
    QRegion bufferDamage() const;
    QRectF bufferSourceBox() const;
    /**
     * Returns the buffer transform that had been applied to the buffer to compensate for
     * output rotation.
     *
     * If the surface is on an output that is rotated 90 degrees clockwise, the buffer will
     * be rotated 90 degrees counter clockwise.
     */
    OutputTransform bufferTransform() const;
    /**
     * @returns the current GraphicsBuffer, might be @c nullptr.
     */
    GraphicsBuffer *buffer() const;
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
    ShadowInterface *shadow() const;

    /**
     * @returns The Blur for this Surface.
     */
    BlurInterface *blur() const;

    /**
     * @returns The Slide for this Surface.
     */
    SlideInterface *slideOnShowHide() const;

    /**
     * @returns The Contrast for this Surface.
     */
    ContrastInterface *contrast() const;

    /**
     * Whether the SurfaceInterface is currently considered to be mapped.
     * A SurfaceInterface is mapped if it has a non-null GraphicsBuffer attached.
     * If the SurfaceInterface references a SubSurfaceInterface it is only considered
     * mapped if it has a GraphicsBuffer attached and the parent SurfaceInterface is mapped.
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
    void setOutputs(const QList<OutputInterface *> &outputs, OutputInterface *primaryOutput);

    /**
     * @returns All OutputInterfaces the SurfaceInterface is on.
     * @see setOutputs
     */
    QList<OutputInterface *> outputs() const;

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
    ContentType contentType() const;

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
    PresentationModeHint presentationModeHint() const;

    /**
     * Sets a preferred buffer scale that clients should provide buffers in
     * @param scale
     */
    void setPreferredBufferScale(qreal scale);

    /**
     * Sets the preferred buffer transform for this surface.
     *
     * This indicates to the client the preferred buffer transform to use when
     * attaching buffers to this surface.
     */
    void setPreferredBufferTransform(OutputTransform transform);

    /**
     * The first committed transaction that is scheduled to be applied to this surface.
     */
    Transaction *firstTransaction() const;
    void setFirstTransaction(Transaction *transaction);

    /**
     * The last committed transaction that is scheduled to be applied to this surface.
     */
    Transaction *lastTransaction() const;
    void setLastTransaction(Transaction *transaction);

    const ColorDescription &colorDescription() const;
    RenderingIntent renderingIntent() const;

    void setPreferredColorDescription(const ColorDescription &descr);

    double alphaMultiplier() const;

    /**
     * Returns the current release point for the buffer on this surface. The buffer keeps the
     * release point referenced as long as it's referenced itself; for synchronization on the
     * GPU side, the compositor has to either keep the release point referenced as long as the
     * GPU task is running, or add a fence for each GPU task to the release point
     */
    std::shared_ptr<SyncReleasePoint> bufferReleasePoint() const;

    /**
     * Traverses the surface sub-tree with this surface as the root.
     */
    void traverseTree(std::function<void(SurfaceInterface *surface)> callback);

    /**
     * @returns the last surface found while traversing the subsurfaces parents
     */
    SurfaceInterface *mainSurface();

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
     * Emitted whenever the SurfaceInterface got damaged.
     * The signal is only emitted during the commit of state.
     * A damage means that a new GraphicsBuffer got attached.
     *
     * @see buffer
     * @see damage
     */
    void damaged(const QRegion &);
    void opaqueChanged(const QRegion &);
    void inputChanged(const QRegion &);
    /**
     * This signal is emitted when the buffer transform has changed.
     */
    void bufferTransformChanged(KWin::OutputTransform);
    void bufferSourceBoxChanged();
    void bufferChanged();
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

    void colorDescriptionChanged();
    void presentationModeHintChanged();
    void bufferReleasePointChanged();
    void alphaMultiplierChanged();

    /**
     * Emitted when the Surface has been committed.
     *
     * This signal is emitted after all the relevant damage and xyzChanged signals
     * for this commit are emitted.
     */
    void committed();

    /**
     * This signal is emitted when a surface commit with the specified \a serial has been cached
     * to be applied later.
     */
    void stateStashed(quint32 serial);

    /**
     * This signal is emitted when the state in a surface commit with the specified \a serial
     * has been applied.
     */
    void stateApplied(quint32 serial);

private:
    std::unique_ptr<SurfaceInterfacePrivate> d;
    friend class SurfaceInterfacePrivate;
};

/**
 * The SurfaceExtension class is the base class for wl_surface extensions. The SurfaceExtension
 * helps with managing extension state and keeping it in sync with the surface state.
 */
template<typename Commit>
class SurfaceExtension : public QObject
{
public:
    explicit SurfaceExtension(SurfaceInterface *surface)
    {
        connect(surface, &SurfaceInterface::stateStashed, this, &SurfaceExtension::stashState);
        connect(surface, &SurfaceInterface::stateApplied, this, &SurfaceExtension::applyState);
    }

    virtual void apply(Commit *commit) = 0;

    Commit pending;
    QMap<quint32, Commit> stashed;

private:
    void stashState(quint32 serial)
    {
        Commit stash = std::exchange(pending, Commit{});
        stashed.insert(serial, stash);
    }

    void applyState(quint32 serial)
    {
        if (!stashed.isEmpty()) {
            if (stashed.firstKey() == serial) {
                Commit stash = stashed.take(serial);
                apply(&stash);
            }
            return;
        }

        apply(&pending);
        pending = Commit{};
    }
};

} // namespace KWin

Q_DECLARE_METATYPE(KWin::SurfaceInterface *)

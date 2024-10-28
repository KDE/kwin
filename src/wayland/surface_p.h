/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/graphicsbuffer.h"
#include "surface.h"
// Qt
#include <QHash>
#include <QList>
#include <QPointer>
// Wayland
#include "qwayland-server-wayland.h"
// C++
#include <deque>
#include <optional>

namespace KWin
{
class IdleInhibitorV1Interface;
class ViewportInterface;
class ContentTypeV1Interface;
class TearingControlV1Interface;
class FractionalScaleV1Interface;
class FrogColorManagementSurfaceV1;
class PresentationTimeFeedback;
class ColorSurfaceV1;
class ColorFeedbackSurfaceV1;
class LinuxDrmSyncObjSurfaceV1;
class AlphaModifierSurfaceV1;

struct SurfaceState
{
    SurfaceState();
    SurfaceState(const SurfaceState &cpy) = delete;
    ~SurfaceState();
    SurfaceState &operator=(SurfaceState &&mv) = default;

    void mergeInto(SurfaceState *target);

    quint32 serial = 0;

    QRegion damage = QRegion();
    QRegion bufferDamage = QRegion();
    QRegion opaque = QRegion();
    QRegion input = infiniteRegion();
    bool inputIsSet = false;
    bool opaqueIsSet = false;
    bool bufferIsSet = false;
    bool shadowIsSet = false;
    bool blurIsSet = false;
    bool contrastIsSet = false;
    bool slideIsSet = false;
    bool subsurfaceOrderChanged = false;
    bool subsurfacePositionChanged = false;
    bool bufferScaleIsSet = false;
    bool bufferTransformIsSet = false;
    bool contentTypeIsSet = false;
    bool presentationModeHintIsSet = false;
    bool colorDescriptionIsSet = false;
    bool alphaMultiplierIsSet = false;
    qint32 bufferScale = 1;
    OutputTransform bufferTransform = OutputTransform::Normal;
    wl_list frameCallbacks;
    QPoint offset = QPoint();
    QPointer<GraphicsBuffer> buffer;
    QPointer<ShadowInterface> shadow;
    QPointer<BlurInterface> blur;
    QPointer<ContrastInterface> contrast;
    QPointer<SlideInterface> slide;
    ContentType contentType = ContentType::None;
    PresentationModeHint presentationHint = PresentationModeHint::VSync;
    ColorDescription colorDescription = ColorDescription::sRGB;
    RenderingIntent renderingIntent = RenderingIntent::Perceptual;
    std::unique_ptr<PresentationTimeFeedback> presentationFeedback;
    struct
    {
        std::shared_ptr<SyncTimeline> timeline;
        uint64_t point = 0;
    } acquirePoint;
    std::shared_ptr<SyncReleasePoint> releasePoint;
    double alphaMultiplier = 1;

    struct
    {
        // Subsurfaces are stored in two lists. The below list contains subsurfaces that
        // are below their parent surface; the above list contains subsurfaces that are
        // placed above the parent surface.
        QList<SubSurfaceInterface *> below;
        QList<SubSurfaceInterface *> above;

        // Subsurface position is here becase it is a part of the parent surface's state.
        QHash<SubSurfaceInterface *, QPoint> position;
    } subsurface;

    struct
    {
        QRectF sourceGeometry = QRectF();
        QSize destinationSize = QSize();
        bool sourceGeometryIsSet = false;
        bool destinationSizeIsSet = false;
    } viewport;
};

class SurfaceInterfacePrivate : public QtWaylandServer::wl_surface
{
public:
    static SurfaceInterfacePrivate *get(SurfaceInterface *surface)
    {
        return surface->d.get();
    }

    explicit SurfaceInterfacePrivate(SurfaceInterface *q);

    void addChild(SubSurfaceInterface *subsurface);
    void removeChild(SubSurfaceInterface *subsurface);
    bool raiseChild(SubSurfaceInterface *subsurface, SurfaceInterface *anchor);
    bool lowerChild(SubSurfaceInterface *subsurface, SurfaceInterface *anchor);
    void setShadow(const QPointer<ShadowInterface> &shadow);
    void setBlur(const QPointer<BlurInterface> &blur);
    void setContrast(const QPointer<ContrastInterface> &contrast);
    void setSlide(const QPointer<SlideInterface> &slide);
    void installPointerConstraint(LockedPointerV1Interface *lock);
    void installPointerConstraint(ConfinedPointerV1Interface *confinement);
    void installIdleInhibitor(IdleInhibitorV1Interface *inhibitor);
    void removeIdleInhibitor(IdleInhibitorV1Interface *inhibitor);

    QRectF computeBufferSourceBox() const;
    void applyState(SurfaceState *next);

    bool computeEffectiveMapped() const;
    void updateEffectiveMapped();

    /**
     * Returns true if this surface (not including subsurfaces) contains a given point
     * @param position in surface-local co-ordiantes
     */
    bool contains(const QPointF &position) const;
    bool inputContains(const QPointF &position) const;
    QRegion mapToBuffer(const QRegion &region) const;

    CompositorInterface *compositor;
    SurfaceInterface *q;
    SurfaceRole *role = nullptr;
    std::unique_ptr<SurfaceState> current;
    std::unique_ptr<SurfaceState> pending;
    QRectF bufferSourceBox;
    QSizeF surfaceSize = QSizeF(0, 0);

    QRegion inputRegion;
    QRegion opaqueRegion;
    GraphicsBufferRef bufferRef;
    QRegion bufferDamage;
    bool mapped = false;
    qreal scaleOverride = 1.;
    qreal pendingScaleOverride = 1.;

    Transaction *firstTransaction = nullptr;
    Transaction *lastTransaction = nullptr;

    QList<OutputInterface *> outputs;
    QPointer<OutputInterface> primaryOutput;
    std::optional<qreal> preferredBufferScale;
    std::optional<OutputTransform> preferredBufferTransform;
    std::optional<ColorDescription> preferredColorDescription;

    LockedPointerV1Interface *lockedPointer = nullptr;
    ConfinedPointerV1Interface *confinedPointer = nullptr;
    QHash<OutputInterface *, QMetaObject::Connection> outputDestroyedConnections;
    QHash<OutputInterface *, QMetaObject::Connection> outputBoundConnections;

    QList<IdleInhibitorV1Interface *> idleInhibitors;
    ViewportInterface *viewportExtension = nullptr;
    std::unique_ptr<LinuxDmaBufV1Feedback> dmabufFeedbackV1;
    QPointer<ContentTypeV1Interface> contentTypeInterface;
    FractionalScaleV1Interface *fractionalScaleExtension = nullptr;
    ClientConnection *client = nullptr;
    TearingControlV1Interface *tearing = nullptr;
    FrogColorManagementSurfaceV1 *frogColorManagement = nullptr;
    ColorSurfaceV1 *colorSurface = nullptr;
    QList<ColorFeedbackSurfaceV1 *> colorFeedbackSurfaces;
    LinuxDrmSyncObjSurfaceV1 *syncObjV1 = nullptr;
    AlphaModifierSurfaceV1 *alphaModifier = nullptr;

    struct
    {
        SubSurfaceInterface *handle = nullptr;
        std::unique_ptr<Transaction> transaction;
    } subsurface;

    std::vector<std::unique_ptr<PresentationTimeFeedback>> pendingPresentationFeedbacks;

protected:
    void surface_destroy_resource(Resource *resource) override;
    void surface_destroy(Resource *resource) override;
    void surface_attach(Resource *resource, struct ::wl_resource *buffer, int32_t x, int32_t y) override;
    void surface_damage(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void surface_frame(Resource *resource, uint32_t callback) override;
    void surface_set_opaque_region(Resource *resource, struct ::wl_resource *region) override;
    void surface_set_input_region(Resource *resource, struct ::wl_resource *region) override;
    void surface_commit(Resource *resource) override;
    void surface_set_buffer_transform(Resource *resource, int32_t transform) override;
    void surface_set_buffer_scale(Resource *resource, int32_t scale) override;
    void surface_damage_buffer(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void surface_offset(Resource *resource, int32_t x, int32_t y) override;

private:
    QMetaObject::Connection constrainsOneShotConnection;
    QMetaObject::Connection constrainsUnboundConnection;
};

} // namespace KWin

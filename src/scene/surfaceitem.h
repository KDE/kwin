/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/drm_formats.h"
#include "core/graphicsbuffer.h"
#include "core/output.h"
#include "scene/item.h"

#include <deque>

namespace KWin
{

class EglBackend;
class GLTexture;
class Texture;
class Window;

/**
 * The SurfaceItem class represents a surface with some contents.
 */
class KWIN_EXPORT SurfaceItem : public Item
{
    Q_OBJECT

public:
    ~SurfaceItem() override;

    QSizeF destinationSize() const;
    void setDestinationSize(const QSizeF &size);

    GraphicsBuffer *buffer() const;
    void setBuffer(GraphicsBuffer *buffer);

    void setBufferReleasePoint(const std::shared_ptr<SyncReleasePoint> &releasePoint);

    RectF bufferSourceBox() const;
    void setBufferSourceBox(const RectF &box);

    OutputTransform bufferTransform() const;
    void setBufferTransform(OutputTransform transform);

    QSize bufferSize() const;
    void setBufferSize(const QSize &size);

    bool hasAlphaChannel() const;

    RegionF mapFromBuffer(const Region &region) const;

    void addDamage(const Region &region);

    Texture *texture(RenderDevice *device) const;

    virtual ContentType contentType() const;
    virtual void setScanoutHint(DrmDevice *device, const FormatModifierMap &drmFormats);

    virtual void freeze();

    /**
     * like frameTimeEstimation, but takes child items into account
     */
    std::optional<std::chrono::nanoseconds> recursiveFrameTimeEstimation() const;
    std::optional<std::chrono::nanoseconds> frameTimeEstimation() const;

Q_SIGNALS:
    void damaged();

protected:
    explicit SurfaceItem(Item *parent = nullptr);

    void resetDamage(RenderDevice *Device);
    Region damage(RenderDevice *device) const;

    void preprocess(ItemRenderer *renderer) override;
    WindowQuadList buildQuads(ItemRenderer *renderer) const override;
    void releaseResources(RenderDevice *device) override;

    OutputTransform m_bufferToSurfaceTransform;
    OutputTransform m_surfaceToBufferTransform;
    GraphicsBufferRef m_bufferRef;
    RectF m_bufferSourceBox;
    QSize m_bufferSize;
    QSizeF m_destinationSize;
    bool m_hasAlphaChannel = false;
    std::unordered_map<RenderDevice *, std::unique_ptr<Texture>> m_textures;
    std::unordered_map<RenderDevice *, Region> m_damage;
    std::deque<std::chrono::nanoseconds> m_lastDamageTimeDiffs;
    std::optional<std::chrono::nanoseconds> m_accumulatedTimeDiffs;
    std::optional<std::chrono::steady_clock::time_point> m_lastDamage;
    std::shared_ptr<SyncReleasePoint> m_bufferReleasePoint;
};

} // namespace KWin

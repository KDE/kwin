/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbuffer.h"
#include "core/output.h"
#include "scene/item.h"

#include <deque>

namespace KWin
{

class EglBackend;
class GLTexture;
class QPainterBackend;
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

    RectF bufferSourceBox() const;
    void setBufferSourceBox(const RectF &box);

    OutputTransform bufferTransform() const;
    void setBufferTransform(OutputTransform transform);

    QSize bufferSize() const;
    void setBufferSize(const QSize &size);

    bool hasAlphaChannel() const;

    std::shared_ptr<SyncReleasePoint> bufferReleasePoint() const;

    Region mapFromBuffer(const Region &region) const;

    void addDamage(const Region &region);
    void resetDamage();
    Region damage() const;

    void destroyTexture();

    Texture *texture() const;

    virtual ContentType contentType() const;
    virtual void setScanoutHint(DrmDevice *device, const QHash<uint32_t, QList<uint64_t>> &drmFormats);

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

    void preprocess() override;
    WindowQuadList buildQuads() const override;
    void releaseResources() override;

    Region m_damage;
    OutputTransform m_bufferToSurfaceTransform;
    OutputTransform m_surfaceToBufferTransform;
    GraphicsBufferRef m_bufferRef;
    RectF m_bufferSourceBox;
    QSize m_bufferSize;
    QSizeF m_destinationSize;
    bool m_hasAlphaChannel = false;
    std::unique_ptr<Texture> m_texture;
    std::deque<std::chrono::nanoseconds> m_lastDamageTimeDiffs;
    std::optional<std::chrono::steady_clock::time_point> m_lastDamage;
    std::optional<std::chrono::nanoseconds> m_frameTimeEstimation;
    std::shared_ptr<SyncReleasePoint> m_bufferReleasePoint;
};

} // namespace KWin

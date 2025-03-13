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

class SurfacePixmap;
class Window;

/**
 * The SurfaceItem class represents a surface with some contents.
 */
class KWIN_EXPORT SurfaceItem : public Item
{
    Q_OBJECT

public:
    QSizeF destinationSize() const;
    void setDestinationSize(const QSizeF &size);

    GraphicsBuffer *buffer() const;
    void setBuffer(GraphicsBuffer *buffer);

    QRectF bufferSourceBox() const;
    void setBufferSourceBox(const QRectF &box);

    OutputTransform bufferTransform() const;
    void setBufferTransform(OutputTransform transform);

    QSize bufferSize() const;
    void setBufferSize(const QSize &size);

    std::shared_ptr<SyncReleasePoint> bufferReleasePoint() const;

    QRegion mapFromBuffer(const QRegion &region) const;

    void addDamage(const QRegion &region);
    void resetDamage();
    QRegion damage() const;

    void destroyPixmap();

    SurfacePixmap *pixmap() const;

    virtual ContentType contentType() const;
    virtual void setScanoutHint(DrmDevice *device, const QHash<uint32_t, QList<uint64_t>> &drmFormats);

    virtual void freeze();

    std::chrono::nanoseconds frameTimeEstimation() const;

Q_SIGNALS:
    void damaged();

protected:
    explicit SurfaceItem(Item *parent = nullptr);

    void preprocess() override;
    WindowQuadList buildQuads() const override;

    QRegion m_damage;
    OutputTransform m_bufferToSurfaceTransform;
    OutputTransform m_surfaceToBufferTransform;
    GraphicsBufferRef m_bufferRef;
    QRectF m_bufferSourceBox;
    QSize m_bufferSize;
    QSizeF m_destinationSize;
    std::unique_ptr<SurfacePixmap> m_pixmap;
    std::deque<std::chrono::nanoseconds> m_lastDamageTimeDiffs;
    std::optional<std::chrono::steady_clock::time_point> m_lastDamage;
    std::chrono::nanoseconds m_frameTimeEstimation = std::chrono::days(1000);
    std::shared_ptr<SyncReleasePoint> m_bufferReleasePoint;
};

class KWIN_EXPORT SurfaceTexture
{
public:
    virtual ~SurfaceTexture();

    virtual bool isValid() const = 0;

    virtual bool create() = 0;
    virtual void update(const QRegion &region) = 0;
};

/**
 * TODO: Drop SurfacePixmap after kwin_wayland and kwin_x11 are split.
 */
class KWIN_EXPORT SurfacePixmap : public QObject
{
    Q_OBJECT

public:
    explicit SurfacePixmap(SurfaceItem *item);

    SurfaceItem *item() const;
    SurfaceTexture *texture() const;

    bool hasAlphaChannel() const;
    QSize size() const;

    void create();
    void update();
    bool isValid() const;

protected:
    SurfaceItem *m_item;
    QSize m_size;
    bool m_valid = false;
    bool m_hasAlphaChannel = false;

private:
    std::unique_ptr<SurfaceTexture> m_texture;
};

} // namespace KWin

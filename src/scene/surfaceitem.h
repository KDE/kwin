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
class SurfaceTexture;
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

    bool hasAlphaChannel() const;

    std::shared_ptr<SyncReleasePoint> bufferReleasePoint() const;

    QRegion mapFromBuffer(const QRegion &region) const;

    void addDamage(const QRegion &region);
    void resetDamage();
    QRegion damage() const;

    void destroyTexture();

    SurfaceTexture *texture() const;

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

    QRegion m_damage;
    OutputTransform m_bufferToSurfaceTransform;
    OutputTransform m_surfaceToBufferTransform;
    GraphicsBufferRef m_bufferRef;
    QRectF m_bufferSourceBox;
    QSize m_bufferSize;
    QSizeF m_destinationSize;
    bool m_hasAlphaChannel = false;
    std::unique_ptr<SurfaceTexture> m_texture;
    std::deque<std::chrono::nanoseconds> m_lastDamageTimeDiffs;
    std::optional<std::chrono::steady_clock::time_point> m_lastDamage;
    std::optional<std::chrono::nanoseconds> m_frameTimeEstimation;
    std::shared_ptr<SyncReleasePoint> m_bufferReleasePoint;
};

class KWIN_EXPORT SurfaceTexture
{
public:
    virtual ~SurfaceTexture();

    virtual bool isValid() const = 0;

    virtual bool create() = 0;
    virtual void update(const QRegion &region) = 0;

    // TODO: create()/update() steps are unnecessary now, consider removing size().
    QSize size() const;

protected:
    QSize m_size;
};

class KWIN_EXPORT OpenGLSurfaceContents
{
public:
    OpenGLSurfaceContents()
    {
    }
    OpenGLSurfaceContents(const std::shared_ptr<GLTexture> &contents)
        : planes({contents})
    {
    }
    OpenGLSurfaceContents(const QList<std::shared_ptr<GLTexture>> &planes)
        : planes(planes)
    {
    }

    void reset()
    {
        planes.clear();
    }
    bool isValid() const
    {
        return !planes.isEmpty();
    }
    QVarLengthArray<GLTexture *, 4> toVarLengthArray() const;

    QList<std::shared_ptr<GLTexture>> planes;
};

class KWIN_EXPORT OpenGLSurfaceTexture : public SurfaceTexture
{
public:
    explicit OpenGLSurfaceTexture(EglBackend *backend, SurfaceItem *item);
    ~OpenGLSurfaceTexture() override;

    bool create() override;
    void update(const QRegion &region) override;
    bool isValid() const override;

    OpenGLSurfaceContents texture() const;

private:
    bool loadShmTexture(GraphicsBuffer *buffer);
    void updateShmTexture(GraphicsBuffer *buffer, const QRegion &region);
    bool loadDmabufTexture(GraphicsBuffer *buffer);
    void updateDmabufTexture(GraphicsBuffer *buffer);
    bool loadSinglePixelTexture(GraphicsBuffer *buffer);
    void updateSinglePixelTexture(GraphicsBuffer *buffer);
    void destroy();

    enum class BufferType {
        None,
        Shm,
        DmaBuf,
        SinglePixel,
    };

    BufferType m_bufferType = BufferType::None;
    EglBackend *m_backend;
    SurfaceItem *m_item;
    OpenGLSurfaceContents m_texture;
};

class KWIN_EXPORT QPainterSurfaceTexture : public SurfaceTexture
{
public:
    QPainterSurfaceTexture(QPainterBackend *backend, SurfaceItem *item);

    bool create() override;
    void update(const QRegion &region) override;
    bool isValid() const override;

    QPainterBackend *backend() const;
    QImage image() const;

protected:
    QPainterBackend *m_backend;
    SurfaceItem *m_item;
    QImage m_image;
};

inline QVarLengthArray<GLTexture *, 4> OpenGLSurfaceContents::toVarLengthArray() const
{
    Q_ASSERT(planes.size() <= 4);
    QVarLengthArray<GLTexture *, 4> ret;
    for (const auto &plane : planes) {
        ret << plane.get();
    }
    return ret;
}

} // namespace KWin

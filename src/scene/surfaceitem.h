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

    QRectF bufferSourceBox() const;
    void setBufferSourceBox(const QRectF &box);

    OutputTransform bufferTransform() const;
    void setBufferTransform(OutputTransform transform);

    QSize bufferSize() const;
    void setBufferSize(const QSize &size);

    QRegion mapFromBuffer(const QRegion &region) const;

    void addDamage(const QRegion &region);
    void resetDamage();
    QRegion damage() const;

    void discardPixmap();
    void updatePixmap();
    void destroyPixmap();

    SurfacePixmap *pixmap() const;
    SurfacePixmap *previousPixmap() const;

    void referencePreviousPixmap();
    void unreferencePreviousPixmap();

    virtual ContentType contentType() const;

    virtual void freeze();

    double refreshRateEstimation() const;

Q_SIGNALS:
    void damaged();

protected:
    explicit SurfaceItem(Scene *scene, Item *parent = nullptr);

    virtual std::unique_ptr<SurfacePixmap> createPixmap() = 0;
    void preprocess() override;
    WindowQuadList buildQuads() const override;

    QRegion m_damage;
    OutputTransform m_bufferToSurfaceTransform;
    OutputTransform m_surfaceToBufferTransform;
    QRectF m_bufferSourceBox;
    QSize m_bufferSize;
    QSizeF m_destinationSize;
    std::unique_ptr<SurfacePixmap> m_pixmap;
    std::unique_ptr<SurfacePixmap> m_previousPixmap;
    int m_referencePixmapCounter = 0;
    std::deque<std::chrono::nanoseconds> m_lastDamageTimeDiffs;
    std::optional<std::chrono::steady_clock::time_point> m_lastDamage;
    double m_refreshRate = 0;
};

class KWIN_EXPORT SurfaceTexture
{
public:
    virtual ~SurfaceTexture();

    virtual bool isValid() const = 0;
};

class KWIN_EXPORT SurfacePixmap : public QObject
{
    Q_OBJECT

public:
    explicit SurfacePixmap(std::unique_ptr<SurfaceTexture> &&texture, QObject *parent = nullptr);

    GraphicsBuffer *buffer() const;
    void setBuffer(GraphicsBuffer *buffer);

    GraphicsBufferOrigin bufferOrigin() const;
    void setBufferOrigin(GraphicsBufferOrigin origin);

    SurfaceTexture *texture() const;

    bool hasAlphaChannel() const;
    QSize size() const;

    bool isDiscarded() const;
    void markAsDiscarded();

    virtual void create() = 0;
    virtual void update();

    virtual bool isValid() const = 0;

protected:
    GraphicsBufferRef m_bufferRef;
    GraphicsBufferOrigin m_bufferOrigin = GraphicsBufferOrigin::TopLeft;
    QSize m_size;
    bool m_hasAlphaChannel = false;

private:
    std::unique_ptr<SurfaceTexture> m_texture;
    bool m_isDiscarded = false;
};

} // namespace KWin

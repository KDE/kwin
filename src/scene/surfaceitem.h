/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "scene/item.h"

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
    QMatrix4x4 surfaceToBufferMatrix() const;
    void setSurfaceToBufferMatrix(const QMatrix4x4 &matrix);

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

Q_SIGNALS:
    void damaged();

protected:
    explicit SurfaceItem(Scene *scene, Item *parent = nullptr);

    virtual std::unique_ptr<SurfacePixmap> createPixmap() = 0;
    void preprocess() override;
    WindowQuadList buildQuads() const override;

    QRegion m_damage;
    std::unique_ptr<SurfacePixmap> m_pixmap;
    std::unique_ptr<SurfacePixmap> m_previousPixmap;
    QMatrix4x4 m_surfaceToBufferMatrix;
    int m_referencePixmapCounter = 0;
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

    SurfaceTexture *texture() const;

    bool hasAlphaChannel() const;
    QSize size() const;

    bool isDiscarded() const;
    void markAsDiscarded();

    virtual void create() = 0;
    virtual void update();

    virtual bool isValid() const = 0;

protected:
    QSize m_size;
    bool m_hasAlphaChannel = false;

private:
    std::unique_ptr<SurfaceTexture> m_texture;
    bool m_isDiscarded = false;
};

} // namespace KWin

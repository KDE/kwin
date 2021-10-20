/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "item.h"

namespace KWin
{

class Deleted;
class SurfacePixmap;
class Toplevel;

/**
 * The SurfaceItem class represents a surface with some contents.
 */
class KWIN_EXPORT SurfaceItem : public Item
{
    Q_OBJECT

public:
    QMatrix4x4 surfaceToBufferMatrix() const;
    void setSurfaceToBufferMatrix(const QMatrix4x4 &matrix);

    Toplevel *window() const;

    virtual QRegion shape() const;
    virtual QRegion opaque() const;

    void addDamage(const QRegion &region);
    void resetDamage();
    QRegion damage() const;

    void discardPixmap();
    void updatePixmap();

    SurfacePixmap *pixmap() const;
    SurfacePixmap *previousPixmap() const;

    void referencePreviousPixmap();
    void unreferencePreviousPixmap();

protected:
    explicit SurfaceItem(Toplevel *window, Item *parent = nullptr);

    virtual SurfacePixmap *createPixmap() = 0;
    void preprocess() override;
    WindowQuadList buildQuads() const override;

    void handleWindowClosed(Toplevel *original, Deleted *deleted);

    Toplevel *m_window;
    QRegion m_damage;
    QScopedPointer<SurfacePixmap> m_pixmap;
    QScopedPointer<SurfacePixmap> m_previousPixmap;
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
    explicit SurfacePixmap(SurfaceTexture *texture, QObject *parent = nullptr);

    SurfaceTexture *texture() const;

    bool hasAlphaChannel() const;
    QSize size() const;
    QRect contentsRect() const;

    bool isDiscarded() const;
    void markAsDiscarded();

    virtual void create() = 0;
    virtual void update();

    virtual bool isValid() const = 0;

protected:
    QSize m_size;
    QRect m_contentsRect;
    bool m_hasAlphaChannel = false;

private:
    QScopedPointer<SurfaceTexture> m_texture;
    bool m_isDiscarded = false;
};

} // namespace KWin

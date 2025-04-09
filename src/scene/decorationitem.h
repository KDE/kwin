/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KDecoration3
{
class Decoration;
}

namespace KWin
{

class GLTexture;
class Window;
class Output;

namespace Decoration
{
class DecoratedWindowImpl;
}

class KWIN_EXPORT DecorationRenderer : public QObject
{
    Q_OBJECT

public:
    virtual void render(const QRegion &region) = 0;
    void invalidate();

    // TODO: Move damage tracking inside DecorationItem.
    QRegion damage() const;
    void addDamage(const QRegion &region);
    void resetDamage();

    qreal effectiveDevicePixelRatio() const;
    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal dpr);

    // Reserve some space for padding. We pad decoration parts to avoid texture bleeding.
    static const int TexturePad = 1;

Q_SIGNALS:
    void damaged(const QRegion &region);

protected:
    explicit DecorationRenderer(Decoration::DecoratedWindowImpl *client);

    Decoration::DecoratedWindowImpl *client() const;

    bool areImageSizesDirty() const
    {
        return m_imageSizesDirty;
    }
    void resetImageSizesDirty()
    {
        m_imageSizesDirty = false;
    }
    void renderToPainter(QPainter *painter, const QRectF &rect);

private:
    QPointer<Decoration::DecoratedWindowImpl> m_client;
    QRegion m_damage;
    qreal m_devicePixelRatio = 1;
    bool m_imageSizesDirty;
};

class SceneOpenGLDecorationRenderer : public DecorationRenderer
{
    Q_OBJECT
public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
        Count
    };
    explicit SceneOpenGLDecorationRenderer(Decoration::DecoratedWindowImpl *client);
    ~SceneOpenGLDecorationRenderer() override;

    void render(const QRegion &region) override;

    GLTexture *texture()
    {
        return m_texture.get();
    }
    GLTexture *texture() const
    {
        return m_texture.get();
    }

private:
    void renderPart(const QRectF &rect, const QRectF &partRect, const QPoint &textureOffset, qreal devicePixelRatio, bool rotated = false);
    static const QMargins texturePadForPart(const QRectF &rect, const QRectF &partRect);
    void resizeTexture();
    int toNativeSize(double size) const;
    std::unique_ptr<GLTexture> m_texture;
};

class SceneQPainterDecorationRenderer : public DecorationRenderer
{
    Q_OBJECT
public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
        Count
    };
    explicit SceneQPainterDecorationRenderer(Decoration::DecoratedWindowImpl *client);

    void render(const QRegion &region) override;

    QImage image(DecorationPart part) const;

private:
    void resizeImages();
    QImage m_images[int(DecorationPart::Count)];
};

/**
 * The DecorationItem class represents a server-side decoration.
 */
class KWIN_EXPORT DecorationItem : public Item
{
    Q_OBJECT

public:
    explicit DecorationItem(KDecoration3::Decoration *decoration, Window *window, Item *parent = nullptr);

    DecorationRenderer *renderer() const;
    Window *window() const;

    QList<QRectF> shape() const override final;
    QRegion opaque() const override final;

private Q_SLOTS:
    void handleDecorationGeometryChanged();
    void updateScale();

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;

private:
    Window *m_window;
    QPointer<Output> m_output;
    QPointer<KDecoration3::Decoration> m_decoration;
    std::unique_ptr<DecorationRenderer> m_renderer;
};

} // namespace KWin

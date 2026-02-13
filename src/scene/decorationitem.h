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

class Atlas;
class ItemRenderer;
class OutlinedBorderItem;
class GLTexture;
class Window;
class LogicalOutput;

namespace Decoration
{
class DecoratedWindowImpl;
}

class KWIN_EXPORT DecorationRenderer : public QObject
{
    Q_OBJECT

public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
    };

    explicit DecorationRenderer(Decoration::DecoratedWindowImpl *client);
    ~DecorationRenderer();

    Atlas *atlas() const;
    void render(ItemRenderer *renderer, const Region &region);
    void invalidate();

    // TODO: Move damage tracking inside DecorationItem.
    Region damage() const;
    void addDamage(const Region &region);
    void resetDamage();

    qreal effectiveDevicePixelRatio() const;
    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal dpr);

    void releaseResources();

Q_SIGNALS:
    void damaged(const Region &region);

private:
    QPointer<Decoration::DecoratedWindowImpl> m_client;
    Region m_damage;
    qreal m_devicePixelRatio = 1;
    bool m_imageSizesDirty;
    QImage m_images[4];
    std::unique_ptr<Atlas> m_atlas;
};

/**
 * The DecorationItem class represents a server-side decoration.
 */
class KWIN_EXPORT DecorationItem : public Item
{
    Q_OBJECT

public:
    explicit DecorationItem(KDecoration3::Decoration *decoration, Window *window, Item *parent = nullptr);
    ~DecorationItem() override;

    Atlas *atlas() const;
    Window *window() const;

    QList<RectF> shape() const override final;
    Region opaque() const override final;

private Q_SLOTS:
    void handleDecorationGeometryChanged();
    void updateScale();
    void updateOutline();

protected:
    void preprocess() override;
    WindowQuadList buildQuads() const override;
    void releaseResources() override;

private:
    Window *m_window;
    QPointer<LogicalOutput> m_output;
    QPointer<KDecoration3::Decoration> m_decoration;
    std::unique_ptr<DecorationRenderer> m_renderer;
    std::unique_ptr<OutlinedBorderItem> m_outlineItem;
};

} // namespace KWin

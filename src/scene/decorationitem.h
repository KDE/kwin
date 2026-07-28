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

    Atlas *atlas(RenderDevice *device) const;
    bool needsRepaint(RenderDevice *device) const;
    void render(ItemRenderer *renderer, const RegionF &region);
    void invalidate();

    // TODO: Move damage tracking inside DecorationItem.
    RegionF damage(RenderDevice *device) const;
    void addDamage(const RegionF &region);
    void resetDamage(RenderDevice *device);

    qreal effectiveDevicePixelRatio() const;
    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal dpr);

    void releaseResources(RenderDevice *device);

Q_SIGNALS:
    void damaged(const RegionF &region);

private:
    QPointer<Decoration::DecoratedWindowImpl> m_client;
    QHash<RenderDevice *, RegionF> m_damage;
    qreal m_devicePixelRatio = 1;
    std::unordered_set<RenderDevice *> m_imageSizesNotDirty;
    QImage m_images[4];
    std::unordered_map<RenderDevice *, std::unique_ptr<Atlas>> m_atlas;
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

    Atlas *atlas(RenderDevice *device) const;
    Window *window() const;

    RegionF shape() const override final;
    RegionF opaque() const override final;

private Q_SLOTS:
    void handleDecorationGeometryChanged();
    void updateScale();
    void updateOutline();

protected:
    void preprocess(ItemRenderer *renderer) override;
    WindowQuadList buildQuads(ItemRenderer *renderer) const override;
    void releaseResources(RenderDevice *device) override;

private:
    Window *m_window;
    QPointer<LogicalOutput> m_output;
    QPointer<KDecoration3::Decoration> m_decoration;
    std::unique_ptr<DecorationRenderer> m_renderer;
    std::unique_ptr<OutlinedBorderItem> m_outlineItem;
};

} // namespace KWin

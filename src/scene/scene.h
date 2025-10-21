/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rendertarget.h"
#include "kwin_export.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QRegion>
#include <QPointer>
#include <memory>

namespace KWin
{

class ItemRenderer;
class Output;
class Scene;
class OutputLayer;
class OutputFrame;
class Item;
class SurfaceItem;

class KWIN_EXPORT RenderView : public QObject
{
    Q_OBJECT
public:
    explicit RenderView(Output *output, OutputLayer *layer);

    Output *output() const;
    OutputLayer *layer() const;

    void setLayer(OutputLayer *layer);

    virtual bool isVisible() const;
    virtual QPointF hotspot() const;
    virtual QRectF viewport() const = 0;
    virtual qreal scale() const;
    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const = 0;
    virtual void frame(OutputFrame *frame) = 0;
    virtual void prePaint() = 0;
    virtual QRegion collectDamage() = 0;
    virtual void paint(const RenderTarget &renderTarget, const QRegion &region) = 0;
    virtual void postPaint() = 0;
    virtual bool shouldRenderItem(Item *item) const;
    virtual bool shouldRenderHole(Item *item) const;
    virtual double desiredHdrHeadroom() const = 0;

    /**
     * add a repaint in layer-local logical coordinates
     */
    void addRepaint(const QRegion &region);
    void scheduleRepaint(Item *item);
    /**
     * @returns true if the layer can be moved with the Item
     * and thus no repaint is necessary
     */
    virtual bool canSkipMoveRepaint(Item *item);

    virtual void setExclusive(bool enable);

protected:
    Output *m_output = nullptr;
    OutputLayer *m_layer = nullptr;
};

class KWIN_EXPORT SceneView : public RenderView
{
    Q_OBJECT
public:
    explicit SceneView(Scene *scene, Output *output, OutputLayer *layer);
    ~SceneView() override;

    Scene *scene() const;
    QRectF viewport() const override;
    qreal scale() const override;

    void setViewport(const QRectF &viewport);
    void setScale(qreal scale);

    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    void frame(OutputFrame *frame) override;
    void prePaint() override;
    QRegion collectDamage() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    void postPaint() override;
    double desiredHdrHeadroom() const override;

    void addExclusiveView(RenderView *view);
    void removeExclusiveView(RenderView *view);
    void addUnderlay(RenderView *view);
    void removeUnderlay(RenderView *view);
    /**
     * @returns whether or not the Item should be rendered for this delegate specifically.
     */
    bool shouldRenderItem(Item *item) const override;
    bool shouldRenderHole(Item *item) const override;

private:
    Scene *m_scene;
    Output *m_output = nullptr;
    OutputLayer *m_layer = nullptr;
    QRectF m_viewport;
    qreal m_scale = 1.0;
    QList<RenderView *> m_exclusiveViews;
    QList<RenderView *> m_underlayViews;
};

class KWIN_EXPORT ItemView : public RenderView
{
public:
    explicit ItemView(SceneView *parentView, Item *item, Output *output, OutputLayer *layer);
    ~ItemView() override;

    QPointF hotspot() const override;
    QRectF viewport() const override;
    bool isVisible() const override;
    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    void frame(OutputFrame *frame) override;
    void prePaint() override;
    QRegion collectDamage() override;
    void postPaint() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    bool shouldRenderItem(Item *item) const override;
    void setExclusive(bool enable) override;
    void setUnderlay(bool underlay);

    Item *item() const;

    virtual bool needsRepaint();
    bool canSkipMoveRepaint(Item *item) override;
    double desiredHdrHeadroom() const override;

protected:
    QRectF calculateViewport(const QRectF &itemRect) const;

    SceneView *const m_parentView;
    const QPointer<Item> m_item;
    bool m_exclusive = false;
    bool m_underlay = false;
};

class KWIN_EXPORT ItemTreeView : public ItemView
{
public:
    explicit ItemTreeView(SceneView *parentView, Item *item, Output *output, OutputLayer *layer);
    ~ItemTreeView() override;

    QRectF viewport() const override;
    bool isVisible() const override;
    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    QRegion collectDamage() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    bool shouldRenderItem(Item *item) const override;
    void setExclusive(bool enable) override;
    bool needsRepaint() override;
    bool canSkipMoveRepaint(Item *item) override;
    double desiredHdrHeadroom() const override;
};

class KWIN_EXPORT Scene : public QObject
{
    Q_OBJECT

public:
    // Flags controlling how painting is done.
    enum {
        // WindowItem (or at least part of it) will be painted opaque.
        PAINT_WINDOW_OPAQUE = 1 << 0,
        // WindowItem (or at least part of it) will be painted translucent.
        PAINT_WINDOW_TRANSLUCENT = 1 << 1,
        // WindowItem will be painted with transformed geometry.
        PAINT_WINDOW_TRANSFORMED = 1 << 2,
        // Paint only a region of the screen (can be optimized, cannot
        // be used together with TRANSFORMED flags).
        PAINT_SCREEN_REGION = 1 << 3,
        // Whole screen will be painted with transformed geometry.
        PAINT_SCREEN_TRANSFORMED = 1 << 4,
        // At least one window will be painted with transformed geometry.
        PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
        // Clear whole background as the very first step, without optimizing it
        PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
    };

    explicit Scene(std::unique_ptr<ItemRenderer> &&renderer);
    ~Scene() override;

    ItemRenderer *renderer() const;

    void addRepaint(const QRegion &region);
    void addRepaint(RenderView *delegate, const QRegion &region);
    void addRepaint(int x, int y, int width, int height);
    void addRepaintFull();
    virtual QRegion damage() const;

    QRect geometry() const;
    void setGeometry(const QRect &rect);

    QList<RenderView *> views() const;
    void addView(RenderView *view);
    void removeView(RenderView *view);

    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const;
    struct OverlayCandidates
    {
        QList<SurfaceItem *> overlays;
        QList<SurfaceItem *> underlays;
    };
    virtual OverlayCandidates overlayCandidates(ssize_t maxTotalCount, ssize_t maxOverlayCount, ssize_t maxUnderlayCount) const = 0;
    virtual void prePaint(SceneView *delegate) = 0;
    virtual QRegion collectDamage() = 0;
    virtual void paint(const RenderTarget &renderTarget, const QRegion &region) = 0;
    virtual void postPaint() = 0;
    virtual void frame(SceneView *delegate, OutputFrame *frame);
    virtual double desiredHdrHeadroom() const;

Q_SIGNALS:
    void viewRemoved(RenderView *delegate);

protected:
    std::unique_ptr<ItemRenderer> m_renderer;
    QList<RenderView *> m_views;
    QRect m_geometry;
};

} // namespace KWin

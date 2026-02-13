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
#include <memory>

namespace KWin
{

class ItemRenderer;
class LogicalOutput;
class Scene;
class OutputLayer;
class OutputFrame;
class Item;
class SurfaceItem;
class Window;

class KWIN_EXPORT RenderView : public QObject
{
    Q_OBJECT
public:
    explicit RenderView(LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer);

    LogicalOutput *logicalOutput() const;
    /**
     * may be nullptr.
     */
    BackendOutput *backendOutput() const;
    OutputLayer *layer() const;

    void setLayer(OutputLayer *layer);

    virtual bool isVisible() const;
    virtual QPointF hotspot() const;
    virtual RectF viewport() const = 0;
    virtual qreal scale() const = 0;
    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const = 0;
    virtual void prePaint() = 0;
    virtual Region collectDamage() = 0;
    virtual void paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const Region &logicalRegion) = 0;
    virtual void postPaint() = 0;
    virtual bool shouldRenderItem(Item *item) const;
    virtual bool shouldRenderHole(Item *item) const;
    virtual double desiredHdrHeadroom() const = 0;

    /**
     * add a repaint in layer-local device coordinates
     */
    void addDeviceRepaint(const Region &deviceRegion);
    void scheduleRepaint(Item *item);
    /**
     * @returns true if the layer can be moved with the Item
     * and thus no repaint is necessary
     */
    virtual bool canSkipMoveRepaint(Item *item);

    virtual void setExclusive(bool enable);

    RectF mapToDeviceCoordinates(const RectF &logicalGeometry) const;
    Rect mapToDeviceCoordinatesAligned(const Rect &logicalGeometry) const;
    Rect mapToDeviceCoordinatesAligned(const RectF &logicalGeometry) const;
    Rect mapToDeviceCoordinatesContained(const Rect &logicalGeometry) const;
    Region mapToDeviceCoordinatesAligned(const Region &logicalGeometry) const;
    Region mapToDeviceCoordinatesContained(const Region &logicalGeometry) const;

    RectF mapFromDeviceCoordinates(const RectF &deviceGeometry) const;
    Rect mapFromDeviceCoordinatesAligned(const Rect &deviceGeometry) const;
    Region mapFromDeviceCoordinatesAligned(const Region &deviceGeometry) const;

    /**
     * @returns Rect(renderOffset(), deviceSize())
     */
    Rect deviceRect() const;
    QSize deviceSize() const;

    QPoint renderOffset() const;
    void setRenderOffset(const QPoint &offset);

protected:
    LogicalOutput *m_logicalOutput = nullptr;
    BackendOutput *m_backendOutput = nullptr;
    OutputLayer *m_layer = nullptr;
    QPoint m_renderOffset;
};

class KWIN_EXPORT SceneView : public RenderView
{
    Q_OBJECT
public:
    explicit SceneView(Scene *scene, LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer);
    ~SceneView() override;

    Scene *scene() const;
    RectF viewport() const override;
    qreal scale() const override;

    void setViewport(const RectF &viewport);
    void setScale(qreal scale);

    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    void prePaint() override;
    Region collectDamage() override;
    void paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const Region &deviceRegion) override;
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

    void addWindowFilter(std::function<bool(Window *)> filter);
    bool shouldHideWindow(Window *window) const;

private:
    Scene *m_scene;
    LogicalOutput *m_logicalOutput = nullptr;
    OutputLayer *m_layer = nullptr;
    RectF m_viewport;
    qreal m_scale = 1.0;
    QList<RenderView *> m_exclusiveViews;
    QList<RenderView *> m_underlayViews;
    QList<std::function<bool(Window *)>> m_windowFilters;
};

class KWIN_EXPORT ItemView : public RenderView
{
public:
    explicit ItemView(SceneView *parentView, Item *item, LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer);
    ~ItemView() override;

    qreal scale() const override;
    QPointF hotspot() const override;
    RectF viewport() const override;
    bool isVisible() const override;
    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    void prePaint() override;
    Region collectDamage() override;
    void postPaint() override;
    void paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const Region &logicalRegion) override;
    bool shouldRenderItem(Item *item) const override;
    void setExclusive(bool enable) override;
    void setUnderlay(bool underlay);

    Item *item() const;

    virtual bool needsRepaint();
    bool canSkipMoveRepaint(Item *item) override;
    double desiredHdrHeadroom() const override;

protected:
    RectF calculateViewport(const RectF &itemRect) const;

    SceneView *const m_parentView;
    const QPointer<Item> m_item;
    bool m_exclusive = false;
    bool m_underlay = false;
};

class KWIN_EXPORT ItemTreeView : public ItemView
{
public:
    explicit ItemTreeView(SceneView *parentView, Item *item, LogicalOutput *logicalOutput, BackendOutput *backendOutput, OutputLayer *layer);
    ~ItemTreeView() override;

    RectF viewport() const override;
    bool isVisible() const override;
    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    Region collectDamage() override;
    void paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const Region &logicalRegion) override;
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

    Scene();
    ~Scene() override;

    ItemRenderer *renderer() const;
    virtual void attachRenderer(std::unique_ptr<ItemRenderer> &&renderer) = 0;
    virtual void detachRenderer() = 0;

    void addLogicalRepaint(const Region &logicalRegion);
    void addLogicalRepaint(RenderView *view, const Region &logicalRegion);
    void addDeviceRepaint(RenderView *view, const Region &deviceRegion);
    void addLogicalRepaint(int x, int y, int width, int height);
    void addRepaintFull();
    virtual Region damage() const;

    Rect geometry() const;
    void setGeometry(const Rect &rect);

    QList<RenderView *> views() const;
    void addView(RenderView *view);
    void removeView(RenderView *view);

    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const;
    struct OverlayCandidates
    {
        QList<Item *> overlays;
        QList<Item *> underlays;
    };
    virtual OverlayCandidates overlayCandidates(ssize_t maxTotalCount, ssize_t maxOverlayCount, ssize_t maxUnderlayCount) const = 0;
    virtual void prePaint(SceneView *view) = 0;
    virtual Region collectDamage() = 0;
    virtual void paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const Region &deviceRegion) = 0;
    virtual void postPaint() = 0;
    virtual void frame(SceneView *delegate, OutputFrame *frame) = 0;
    virtual double desiredHdrHeadroom() const = 0;

Q_SIGNALS:
    void viewRemoved(RenderView *delegate);

protected:
    void releaseResources(Item *item);

    std::unique_ptr<ItemRenderer> m_renderer;
    QList<RenderView *> m_views;
    Rect m_geometry;
};

} // namespace KWin

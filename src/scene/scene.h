/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QRegion>
#include <QPointer>
#include <memory>

#include "core/rendertarget.h"
#include "kwin_export.h"

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
    qreal scale() const;
    OutputLayer *layer() const;

    virtual bool isVisible() const;
    virtual QPointF hotspot() const;
    virtual QRectF viewport() const = 0;
    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const = 0;
    virtual SurfaceItem *overlayCandidate() const;
    virtual void frame(OutputFrame *frame) = 0;
    virtual QRegion prePaint() = 0;
    virtual QRegion updatePrePaint() = 0;
    virtual void paint(const RenderTarget &renderTarget, const QRegion &region) = 0;
    virtual void postPaint() = 0;
    virtual bool shouldRenderItem(Item *item) const;

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

    virtual void setEnabled(bool enable);

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

    QRectF viewport() const override;
    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    SurfaceItem *overlayCandidate() const override;
    void frame(OutputFrame *frame) override;
    QRegion prePaint() override;
    QRegion updatePrePaint() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    void postPaint() override;
    double desiredHdrHeadroom() const;

    void hideItem(Item *item);
    void showItem(Item *item);
    /**
     * @returns whether or not the Item should be rendered for this delegate specifically.
     * If it returns true, the Item is already rendered in some other way (like on a hardware plane)
     */
    bool shouldRenderItem(Item *item) const override;

    Scene *scene() const;

private:
    Scene *m_scene;
    Output *m_output = nullptr;
    OutputLayer *m_layer = nullptr;
    QList<Item *> m_hiddenItems;
};

class KWIN_EXPORT ItemTreeView : public RenderView
{
public:
    explicit ItemTreeView(SceneView *parentView, Item *item, Output *output, OutputLayer *layer);
    ~ItemTreeView() override;

    QPointF hotspot() const override;
    QRectF viewport() const override;
    bool isVisible() const override;
    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    void frame(OutputFrame *frame) override;
    QRegion prePaint() override;
    QRegion updatePrePaint() override;
    void postPaint() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    bool shouldRenderItem(Item *item) const override;

    Item *item() const;

    bool needsRepaint();
    bool canSkipMoveRepaint(Item *item) override;
    void setEnabled(bool enable) override;

private:
    SceneView *const m_parentView;
    const QPointer<Item> m_item;
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
    virtual SurfaceItem *overlayCandidate() const = 0;
    virtual QRegion prePaint(SceneView *delegate) = 0;
    /**
     * While prePaint returns damage, some layer-related actions may cause
     * damage to be added after prePaint
     */
    virtual QRegion updatePrePaint() = 0;
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

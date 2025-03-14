/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QRegion>

#include <memory>

namespace KWin
{

class ItemRenderer;
class Output;
class Scene;
class OutputFrame;
class OutputLayer;
class SurfaceItem;
class RenderTarget;
class Item;
class ItemTreeView;

class KWIN_EXPORT SceneView
{
public:
    virtual ~SceneView() = default;

    OutputLayer *layer() const;
    void setLayer(OutputLayer *layer);

    /**
     * This function is called by the compositor after compositing the frame.
     */
    virtual void frame(OutputFrame *frame);

    /**
     * This function is called by the compositor before starting painting. Reimplement
     * this function to do frame initialization.
     */
    virtual QRegion prePaint();

    /**
     * This function is called by the compositor after finishing painting. Reimplement
     * this function to do post frame cleanup.
     */
    virtual void postPaint();

    /**
     * Returns the direct scanout candidate hint. It can be used to avoid compositing the
     * render layer.
     */
    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const;

    /**
     * This function is called when the compositor wants the render layer delegate
     * to repaint its contents.
     */
    virtual void paint(const RenderTarget &renderTarget, const QRegion &region) = 0;

    virtual double desiredHdrHeadroom() const;

    virtual QRect viewport() const = 0;
    virtual void addRepaint(const QRegion &region) = 0;
    virtual void scheduleRepaint(Item *item) = 0;

    void accumulateRepaints(Item *item, QRegion *repaints);

protected:
    OutputLayer *m_layer = nullptr;
};

/**
 * View for the main scene with items and effects
 */
class KWIN_EXPORT MainSceneView : public SceneView
{
public:
    explicit MainSceneView(Scene *scene, Output *output);
    ~MainSceneView() override;

    Output *output() const;
    qreal scale() const;
    QRect viewport() const override;

    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    void frame(OutputFrame *frame) override;
    QRegion prePaint() override;
    void postPaint() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    double desiredHdrHeadroom() const override;

    void addRepaint(const QRegion &region) override;
    void scheduleRepaint(Item *item) override;

    void addItemView(ItemTreeView *view);
    void removeItemView(ItemTreeView *view);
    bool hasItemViewFor(Item *item) const;

    Scene *scene() const;

private:
    Scene *m_scene;
    Output *m_output = nullptr;
    QList<ItemTreeView *> m_itemViews;
};

/**
 * View for an item tree taken out of a MainSceneView
 */
class KWIN_EXPORT ItemTreeView : public SceneView
{
public:
    explicit ItemTreeView(MainSceneView *mainView, Item *item, OutputLayer *layer);
    ~ItemTreeView() override;

    void setViewport(const QRect &rect);

    QRect viewport() const override;
    void addRepaint(const QRegion &region) override;
    void scheduleRepaint(Item *item) override;

    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    QRegion prePaint() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    void postPaint() override;
    void frame(OutputFrame *frame) override;
    double desiredHdrHeadroom() const override;

    Item *item() const;

private:
    MainSceneView *const m_mainView;
    Item *const m_item;
    QRect m_viewport;
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
    void addRepaint(SceneView *delegate, const QRegion &region);
    void addRepaint(int x, int y, int width, int height);
    void addRepaintFull();
    virtual QRegion damage() const;

    QRect geometry() const;
    void setGeometry(const QRect &rect);

    QList<MainSceneView *> delegates() const;
    void addDelegate(MainSceneView *delegate);
    void removeDelegate(MainSceneView *delegate);

    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const;
    virtual Item *overlayCandidate() const;
    virtual QRegion prePaint(MainSceneView *delegate) = 0;
    virtual void postPaint() = 0;
    virtual void paint(const RenderTarget &renderTarget, const QRegion &region) = 0;
    virtual void frame(MainSceneView *delegate, OutputFrame *frame);
    virtual double desiredHdrHeadroom() const;

Q_SIGNALS:
    void delegateRemoved(MainSceneView *delegate);

protected:
    std::unique_ptr<ItemRenderer> m_renderer;
    QList<MainSceneView *> m_delegates;
    QRect m_geometry;
};

} // namespace KWin

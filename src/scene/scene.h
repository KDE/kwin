/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rendertarget.h"
#include "kwin_export.h"

#include <QList>
#include <QObject>
#include <QRegion>
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

class KWIN_EXPORT SceneView : public QObject
{
    Q_OBJECT
public:
    explicit SceneView(Scene *scene, Output *output, OutputLayer *layer);
    ~SceneView() override;

    OutputLayer *layer() const;

    Output *output() const;
    qreal scale() const;
    QRect viewport() const;

    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const;
    void frame(OutputFrame *frame);
    QRegion prePaint();
    void postPaint();
    void paint(const RenderTarget &renderTarget, const QRegion &region);
    double desiredHdrHeadroom() const;

    void hideItem(Item *item);
    void showItem(Item *item);
    /**
     * @returns whether or not the Item should be rendered for this delegate specifically.
     * If it returns true, the Item is already rendered in some other way (like on a hardware plane)
     */
    bool shouldRenderItem(Item *item) const;

    /**
     * add a repaint in layer-local logical coordinates
     */
    void addRepaint(const QRegion &region);
    void scheduleRepaint(Item *item);

private:
    Scene *m_scene;
    Output *m_output = nullptr;
    OutputLayer *m_layer = nullptr;
    QList<Item *> m_hiddenItems;
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

    QList<SceneView *> views() const;
    void addView(SceneView *delegate);
    void removeView(SceneView *delegate);

    virtual QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const;
    virtual QRegion prePaint(SceneView *delegate) = 0;
    virtual void postPaint() = 0;
    virtual void paint(const RenderTarget &renderTarget, const QRegion &region) = 0;
    virtual void frame(SceneView *delegate, OutputFrame *frame);
    virtual double desiredHdrHeadroom() const;

Q_SIGNALS:
    void delegateRemoved(SceneView *delegate);

protected:
    std::unique_ptr<ItemRenderer> m_renderer;
    QList<SceneView *> m_delegates;
    QRect m_geometry;
};

} // namespace KWin

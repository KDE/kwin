/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/renderlayerdelegate.h"

#include <QObject>

#include <memory>

namespace KWin
{

class ItemRenderer;
class Output;
class Scene;

class KWIN_EXPORT SceneDelegate : public RenderLayerDelegate
{
public:
    explicit SceneDelegate(Scene *scene);
    explicit SceneDelegate(Scene *scene, Output *output);
    ~SceneDelegate() override;

    Output *output() const;
    QRect viewport() const;

    QRegion repaints() const override;
    SurfaceItem *scanoutCandidate() const override;
    void prePaint() override;
    void postPaint() override;
    void paint(RenderTarget *renderTarget, const QRegion &region) override;

private:
    Scene *m_scene;
    Output *m_output = nullptr;
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
    void addRepaint(int x, int y, int width, int height);
    void addRepaintFull();
    virtual QRegion damage() const;

    QRect geometry() const;
    void setGeometry(const QRect &rect);

    QList<SceneDelegate *> delegates() const;
    void addDelegate(SceneDelegate *delegate);
    void removeDelegate(SceneDelegate *delegate);

    virtual SurfaceItem *scanoutCandidate() const;
    virtual void prePaint(SceneDelegate *delegate) = 0;
    virtual void postPaint() = 0;
    virtual void paint(RenderTarget *renderTarget, const QRegion &region) = 0;

Q_SIGNALS:
    void delegateRemoved(SceneDelegate *delegate);

protected:
    std::unique_ptr<ItemRenderer> m_renderer;
    QList<SceneDelegate *> m_delegates;
    QRect m_geometry;
};

} // namespace KWin

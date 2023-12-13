/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KDecoration2
{
class Decoration;
}

namespace KWin
{
class Window;
class DecorationItem;
class EffectWindow;
class InternalWindow;
class Shadow;
class ShadowItem;
class SurfaceItem;
class X11Window;

/**
 * The WindowItem class represents a window in the scene.
 *
 * A WindowItem is made of a surface with client contents and optionally a server-side frame
 * and a drop-shadow.
 */
class KWIN_EXPORT WindowItem : public Item
{
    Q_OBJECT

public:
    enum {
        PAINT_DISABLED_BY_HIDDEN = 1 << 0,
        PAINT_DISABLED_BY_DESKTOP = 1 << 1,
        PAINT_DISABLED_BY_MINIMIZE = 1 << 2,
        PAINT_DISABLED_BY_ACTIVITY = 1 << 3,
    };

    ~WindowItem() override;

    SurfaceItem *surfaceItem() const;
    DecorationItem *decorationItem() const;
    ShadowItem *shadowItem() const;
    Window *window() const;
    EffectWindow *effectWindow() const;

    void refVisible(int reason);
    void unrefVisible(int reason);

    void elevate();
    void deelevate();

protected:
    explicit WindowItem(Window *window, Scene *scene, Item *parent = nullptr);
    void updateSurfaceItem(std::unique_ptr<SurfaceItem> &&surfaceItem);

private Q_SLOTS:
    void updateDecorationItem();
    void updateShadowItem();
    void updateSurfacePosition();
    void updateSurfaceVisibility();
    void updatePosition();
    void updateOpacity();
    void updateStackingOrder();
    void addSurfaceItemDamageConnects(Item *item);

private:
    bool computeVisibility() const;
    void updateVisibility();
    void markDamaged();
    void freeze();

    Window *m_window;
    std::unique_ptr<SurfaceItem> m_surfaceItem;
    std::unique_ptr<DecorationItem> m_decorationItem;
    std::unique_ptr<ShadowItem> m_shadowItem;
    std::unique_ptr<EffectWindow> m_effectWindow;
    std::optional<int> m_elevation;
    int m_forceVisibleByHiddenCount = 0;
    int m_forceVisibleByDesktopCount = 0;
    int m_forceVisibleByMinimizeCount = 0;
    int m_forceVisibleByActivityCount = 0;
};

/**
 * The WindowItemX11 class represents an X11 window (both on X11 and Wayland sessions).
 *
 * Note that Xwayland windows and Wayland surfaces are associated asynchronously. This means
 * that the surfaceItem() function can return @c null until the window is fully initialized.
 */
class KWIN_EXPORT WindowItemX11 : public WindowItem
{
    Q_OBJECT

public:
    explicit WindowItemX11(X11Window *window, Scene *scene, Item *parent = nullptr);

private Q_SLOTS:
    void initialize();
};

/**
 * The WindowItemWayland class represents a Wayland window.
 */
class KWIN_EXPORT WindowItemWayland : public WindowItem
{
    Q_OBJECT

public:
    explicit WindowItemWayland(Window *window, Scene *scene, Item *parent = nullptr);
};

/**
 * The WindowItemInternal class represents a window created by the compositor, for
 * example, the task switcher, etc.
 */
class KWIN_EXPORT WindowItemInternal : public WindowItem
{
    Q_OBJECT

public:
    explicit WindowItemInternal(InternalWindow *window, Scene *scene, Item *parent = nullptr);
};

} // namespace KWin

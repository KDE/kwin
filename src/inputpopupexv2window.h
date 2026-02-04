/*
    SPDX-FileCopyrightText: 2026 KWin

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandwindow.h"

#include <QPointer>
#include <QTimer>

namespace KWin
{

class XXInputPopupSurfaceV2Interface;

class InputPopupWindowExV2 : public WaylandWindow
{
    Q_OBJECT
public:
    explicit InputPopupWindowExV2(XXInputPopupSurfaceV2Interface *popupSurface);

    void destroyWindow() override;
    bool isPlaceable() const override
    {
        return false;
    }
    bool isCloseable() const override
    {
        return false;
    }
    bool isResizable() const override
    {
        return false;
    }
    bool isMovable() const override
    {
        return false;
    }
    bool isMovableAcrossScreens() const override
    {
        return false;
    }
    bool acceptsFocus() const override
    {
        return false;
    }
    void closeWindow() override
    {
    }
    bool wantsInput() const override
    {
        return false;
    }
    bool isInputMethod() const override
    {
        return true;
    }

    WindowType windowType() const override;
    RectF frameRectToBufferRect(const RectF &rect) const override;

protected:
    void moveResizeInternal(const RectF &rect, MoveResizeMode mode) override;
    void doSetNextTargetScale() override;
    void doSetPreferredBufferTransform() override;
    void doSetPreferredColorDescription() override;

private:
    void handleMapped();
    void handleUnmapped();
    void reposition();
    void resetPosition();

    RectF m_windowGeometry;
    bool m_wasEverMapped = false;
    const QPointer<XXInputPopupSurfaceV2Interface> m_popupSurface;
    QTimer m_rescalingTimer;
};

} // namespace KWin

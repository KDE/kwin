/*
    SPDX-FileCopyrightText: 2026 KWin

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpopupexv2window.h"

#include "core/output.h"
#include "core/pixelgrid.h"
#include "inputmethod.h"
#include "wayland/surface.h"
#include "wayland/xx_input_method_v2.h"
#include "wayland_server.h"
#include "workspace.h"

namespace KWin
{

InputPopupWindowExV2::InputPopupWindowExV2(XXInputPopupSurfaceV2Interface *popupSurface)
    : WaylandWindow(popupSurface->surface())
    , m_popupSurface(popupSurface)
{
    setOutput(workspace()->activeOutput());
    setMoveResizeOutput(workspace()->activeOutput());
    setSkipSwitcher(true);
    setSkipPager(true);
    setSkipTaskbar(true);

    connect(surface(), &SurfaceInterface::aboutToBeDestroyed, this, &InputPopupWindowExV2::destroyWindow);
    connect(surface(), &SurfaceInterface::sizeChanged, this, &InputPopupWindowExV2::reposition);
    connect(surface(), &SurfaceInterface::inputChanged, this, &InputPopupWindowExV2::reposition);
    connect(surface(), &SurfaceInterface::mapped, this, &InputPopupWindowExV2::handleMapped);
    connect(surface(), &SurfaceInterface::unmapped, this, &InputPopupWindowExV2::handleUnmapped);

    connect(workspace(), &Workspace::outputsChanged, this, &InputPopupWindowExV2::reposition);
    connect(kwinApp()->inputMethod(), &InputMethod::cursorRectangleChanged, this, &InputPopupWindowExV2::reposition);

    m_rescalingTimer.setSingleShot(true);
    m_rescalingTimer.setInterval(0);
    connect(&m_rescalingTimer, &QTimer::timeout, this, [this]() {
        setTargetScale(nextTargetScale());
    });
    connect(&m_rescalingTimer, &QTimer::timeout, this, &InputPopupWindowExV2::reposition);
}

void InputPopupWindowExV2::destroyWindow()
{
    if (m_popupSurface) {
        m_popupSurface->disconnect(this);
    }
    if (surface()) {
        surface()->disconnect(this);
    }

    disconnect(workspace(), &Workspace::outputsChanged, this, &InputPopupWindowExV2::reposition);
    disconnect(kwinApp()->inputMethod(), &InputMethod::cursorRectangleChanged, this, &InputPopupWindowExV2::reposition);

    markAsDeleted();
    Q_EMIT closed();

    m_rescalingTimer.stop();
    StackingUpdatesBlocker blocker(workspace());
    waylandServer()->removeWindow(this);

    unref();
}

WindowType InputPopupWindowExV2::windowType() const
{
    return WindowType::Utility;
}

RectF InputPopupWindowExV2::frameRectToBufferRect(const RectF &rect) const
{
    return RectF(rect.topLeft() - m_windowGeometry.topLeft(), surface()->size());
}

void InputPopupWindowExV2::moveResizeInternal(const RectF &rect, MoveResizeMode)
{
    updateGeometry(rect);
}

void InputPopupWindowExV2::doSetNextTargetScale()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredBufferScale(nextTargetScale());
    m_rescalingTimer.start();
}

void InputPopupWindowExV2::doSetPreferredBufferTransform()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredBufferTransform(preferredBufferTransform());
}

void InputPopupWindowExV2::doSetPreferredColorDescription()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredColorDescription(preferredColorDescription());
}

void InputPopupWindowExV2::handleMapped()
{
    m_wasEverMapped = true;
    reposition();
}

void InputPopupWindowExV2::handleUnmapped()
{
    setHidden(true);
}

void InputPopupWindowExV2::resetPosition()
{
    Q_ASSERT(!isDeleted());

    const RectF cursorRectangle = kwinApp()->inputMethod()->cursorRectangle();
    const RectF screen = Workspace::self()->clientArea(PlacementArea, this, cursorRectangle.bottomLeft());

    m_windowGeometry = RectF(QPointF(0, 0), surface()->size());

    RectF popupRect(cursorRectangle.left(),
                    cursorRectangle.top() + cursorRectangle.height(),
                    m_windowGeometry.width(),
                    m_windowGeometry.height());
    if (popupRect.left() < screen.left()) {
        popupRect.moveLeft(screen.left());
    }
    if (popupRect.right() > screen.right()) {
        popupRect.moveRight(screen.right());
    }
    if (popupRect.top() < screen.top() || popupRect.bottom() > screen.bottom()) {
        const RectF flippedPopupRect(cursorRectangle.left(),
                                     cursorRectangle.top() - m_windowGeometry.height(),
                                     m_windowGeometry.width(),
                                     m_windowGeometry.height());

        if (flippedPopupRect.top() >= screen.top() && flippedPopupRect.bottom() <= screen.bottom()) {
            popupRect.moveTop(flippedPopupRect.top());
        }
    }
    if (popupRect.top() < screen.top()) {
        popupRect.moveTop(screen.top());
    }
    if (popupRect.bottom() > screen.bottom()) {
        popupRect.moveBottom(screen.bottom());
    }

    moveResize(snapToPixels(popupRect, targetScale()));

    // Send a configure sequence to the client describing the geometry and anchor
    if (m_popupSurface) {
        const int32_t anchorX = std::round(cursorRectangle.left() - popupRect.left());
        const int32_t anchorY = std::round(cursorRectangle.top() - popupRect.top());
        const uint32_t anchorW = std::round(cursorRectangle.width());
        const uint32_t anchorH = std::round(cursorRectangle.height());
        const uint32_t width = std::round(popupRect.width());
        const uint32_t height = std::round(popupRect.height());
        m_popupSurface->sendStartConfigure(width, height, anchorX, anchorY, anchorW, anchorH);
        // The input method object should follow with a done; handled by the backend.
    }
}

void InputPopupWindowExV2::reposition()
{
    if (!readyForPainting()) {
        return;
    }
    resetPosition();
    markAsMapped();
    setHidden(false);
}

} // namespace KWin

#include "moc_inputpopupexv2window.cpp"

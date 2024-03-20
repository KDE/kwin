/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpanelv1window.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "inputmethod.h"
#include "wayland/output.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland/textinput_v1.h"
#include "wayland/textinput_v2.h"
#include "wayland/textinput_v3.h"
#include "wayland_server.h"
#include "workspace.h"

namespace KWin
{

InputPanelV1Window::InputPanelV1Window(InputPanelSurfaceV1Interface *panelSurface)
    : WaylandWindow(panelSurface->surface())
    , m_panelSurface(panelSurface)
{
    setOutput(workspace()->activeOutput());
    setMoveResizeOutput(workspace()->activeOutput());
    setSkipSwitcher(true);
    setSkipPager(true);
    setSkipTaskbar(true);

    connect(surface(), &SurfaceInterface::aboutToBeDestroyed, this, &InputPanelV1Window::destroyWindow);
    connect(surface(), &SurfaceInterface::sizeChanged, this, &InputPanelV1Window::reposition);
    connect(surface(), &SurfaceInterface::inputChanged, this, &InputPanelV1Window::reposition);
    connect(surface(), &SurfaceInterface::mapped, this, &InputPanelV1Window::handleMapped);

    connect(panelSurface, &InputPanelSurfaceV1Interface::topLevel, this, &InputPanelV1Window::showTopLevel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::overlayPanel, this, &InputPanelV1Window::showOverlayPanel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::aboutToBeDestroyed, this, &InputPanelV1Window::destroyWindow);

    connect(workspace(), &Workspace::outputsChanged, this, &InputPanelV1Window::reposition);

    m_rescalingTimer.setSingleShot(true);
    m_rescalingTimer.setInterval(0);
    connect(&m_rescalingTimer, &QTimer::timeout, this, [this]() {
        setTargetScale(nextTargetScale());
    });
    connect(&m_rescalingTimer, &QTimer::timeout, this, &InputPanelV1Window::reposition);

    kwinApp()->inputMethod()->setPanel(this);
}

void InputPanelV1Window::showOverlayPanel()
{
    m_mode = Mode::Overlay;
    maybeShow();
}

void InputPanelV1Window::showTopLevel(OutputInterface *output, InputPanelSurfaceV1Interface::Position position)
{
    m_mode = Mode::VirtualKeyboard;
    maybeShow();
}

void InputPanelV1Window::allow()
{
    m_allowed = true;
    maybeShow();
}

void InputPanelV1Window::show()
{
    m_virtualKeyboardShouldBeShown = true;
    maybeShow();
}

void InputPanelV1Window::hide()
{
    m_virtualKeyboardShouldBeShown = false;
    if (readyForPainting() && m_mode != Mode::Overlay) {
        setHidden(true);
    }
}

void InputPanelV1Window::resetPosition()
{
    Q_ASSERT(!isDeleted());
    switch (m_mode) {
    case Mode::None: {
        // should never happen
    }; break;
    case Mode::VirtualKeyboard: {
        // maliit creates a fullscreen overlay so use the input shape as the window geometry.
        m_windowGeometry = surface()->input().boundingRect();

        const auto activeOutput = workspace()->activeOutput();
        QRectF availableArea;
        if (waylandServer()->isScreenLocked()) {
            availableArea = workspace()->clientArea(FullScreenArea, this, activeOutput);
        } else {
            availableArea = workspace()->clientArea(MaximizeArea, this, activeOutput);
        }

        QRectF geo = m_windowGeometry;

        // if it fits, align within available area
        if (geo.width() < availableArea.width()) {
            geo.moveLeft(availableArea.left() + (availableArea.width() - geo.width()) / 2);
        } else { // otherwise align to be centred within the screen
            const QRectF outputArea = activeOutput->geometry();
            geo.moveLeft(outputArea.left() + (outputArea.width() - geo.width()) / 2);
        }

        geo.moveBottom(availableArea.bottom());

        moveResize(snapToPixels(geo, targetScale()));
    } break;
    case Mode::Overlay: {
        auto textInputSurface = waylandServer()->seat()->focusedTextInputSurface();
        auto textWindow = waylandServer()->findWindow(textInputSurface);
        QRect cursorRectangle;
        auto textInputV1 = waylandServer()->seat()->textInputV1();
        if (textInputV1 && textInputV1->isEnabled() && textInputV1->surface() == textInputSurface) {
            cursorRectangle = textInputV1->cursorRectangle();
        }
        auto textInputV2 = waylandServer()->seat()->textInputV2();
        if (textInputV2 && textInputV2->isEnabled() && textInputV2->surface() == textInputSurface) {
            cursorRectangle = textInputV2->cursorRectangle();
        }
        auto textInputV3 = waylandServer()->seat()->textInputV3();
        if (textInputV3 && textInputV3->isEnabled() && textInputV3->surface() == textInputSurface) {
            cursorRectangle = textInputV3->cursorRectangle();
        }
        if (textWindow) {
            cursorRectangle.translate(textWindow->bufferGeometry().topLeft().toPoint());
            const QRectF screen = Workspace::self()->clientArea(PlacementArea, this, cursorRectangle.bottomLeft());

            m_windowGeometry = QRectF(QPointF(0, 0), surface()->size());

            QRectF popupRect(cursorRectangle.left(),
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
                const QRectF flippedPopupRect(cursorRectangle.left(),
                                              cursorRectangle.top() - m_windowGeometry.height(),
                                              m_windowGeometry.width(),
                                              m_windowGeometry.height());

                // if it still doesn't fit we should continue with the unflipped version
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
        }
    } break;
    }
}

void InputPanelV1Window::reposition()
{
    if (!readyForPainting()) {
        return;
    }

    resetPosition();
}

void InputPanelV1Window::destroyWindow()
{
    m_panelSurface->disconnect(this);
    m_panelSurface->surface()->disconnect(this);
    disconnect(workspace(), &Workspace::outputsChanged, this, &InputPanelV1Window::reposition);

    markAsDeleted();
    Q_EMIT closed();

    m_rescalingTimer.stop();
    StackingUpdatesBlocker blocker(workspace());
    waylandServer()->removeWindow(this);

    unref();
}

WindowType InputPanelV1Window::windowType() const
{
    return WindowType::Utility;
}

QRectF InputPanelV1Window::frameRectToBufferRect(const QRectF &rect) const
{
    return QRectF(rect.topLeft() - m_windowGeometry.topLeft(), surface()->size());
}

void InputPanelV1Window::moveResizeInternal(const QRectF &rect, MoveResizeMode mode)
{
    updateGeometry(rect);
}

void InputPanelV1Window::doSetNextTargetScale()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredBufferScale(nextTargetScale());
    // re-align the surface with the new target scale
    m_rescalingTimer.start();
}

void InputPanelV1Window::doSetPreferredBufferTransform()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredBufferTransform(preferredBufferTransform());
}

void InputPanelV1Window::doSetPreferredColorDescription()
{
    if (isDeleted()) {
        return;
    }
    surface()->setPreferredColorDescription(preferredColorDescription());
}

void InputPanelV1Window::handleMapped()
{
    maybeShow();
}

void InputPanelV1Window::maybeShow()
{
    const bool shouldShow = m_mode == Mode::Overlay || (m_mode == Mode::VirtualKeyboard && m_allowed && m_virtualKeyboardShouldBeShown);
    if (shouldShow && !isDeleted() && surface()->isMapped()) {
        resetPosition();
        markAsMapped();
        setHidden(false);
    }
}

} // namespace KWin

#include "moc_inputpanelv1window.cpp"

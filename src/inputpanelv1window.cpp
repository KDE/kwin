/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpanelv1window.h"
#include "core/output.h"
#include "deleted.h"
#include "inputmethod.h"
#include "wayland/output_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland/textinput_v1_interface.h"
#include "wayland/textinput_v2_interface.h"
#include "wayland/textinput_v3_interface.h"
#include "wayland_server.h"
#include "workspace.h"

using namespace KWaylandServer;

namespace KWin
{

InputPanelV1Window::InputPanelV1Window(InputPanelSurfaceV1Interface *panelSurface)
    : WaylandWindow(panelSurface->surface())
    , m_panelSurface(panelSurface)
{
    setSkipSwitcher(true);
    setSkipPager(true);
    setSkipTaskbar(true);

    connect(surface(), &SurfaceInterface::aboutToBeDestroyed, this, &InputPanelV1Window::destroyWindow);
    connect(surface(), &SurfaceInterface::sizeChanged, this, &InputPanelV1Window::reposition);
    connect(surface(), &SurfaceInterface::mapped, this, &InputPanelV1Window::handleMapped);

    connect(panelSurface, &InputPanelSurfaceV1Interface::topLevel, this, &InputPanelV1Window::showTopLevel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::overlayPanel, this, &InputPanelV1Window::showOverlayPanel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::destroyed, this, &InputPanelV1Window::destroyWindow);

    connect(workspace(), &Workspace::outputsChanged, this, &InputPanelV1Window::reposition);

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
        hideClient();
    }
}

void KWin::InputPanelV1Window::reposition()
{
    if (!readyForPainting()) {
        return;
    }

    switch (m_mode) {
    case Mode::None: {
        // should never happen
    }; break;
    case Mode::VirtualKeyboard: {
        QSizeF panelSize = surface()->size();
        if (!panelSize.isValid() || panelSize.isEmpty()) {
            return;
        }

        const auto activeOutput = workspace()->activeOutput();
        const QRectF outputArea = activeOutput->geometry();
        QRectF availableArea;
        if (waylandServer()->isScreenLocked()) {
            availableArea = outputArea;
        } else {
            availableArea = workspace()->clientArea(MaximizeArea, this, activeOutput);
        }

        panelSize = panelSize.boundedTo(availableArea.size());
        QRectF geo(QPointF(availableArea.left(), availableArea.top() + availableArea.height() - panelSize.height()), panelSize);
        geo.translate((availableArea.width() - panelSize.width()) / 2, availableArea.height() - outputArea.height());
        moveResize(geo);
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

            // Reuse the similar logic like xdg popup
            QRectF popupRect(popupOffset(cursorRectangle, Qt::BottomEdge | Qt::LeftEdge, Qt::RightEdge | Qt::BottomEdge, surface()->size()), surface()->size());

            if (popupRect.left() < screen.left()) {
                popupRect.moveLeft(screen.left());
            }
            if (popupRect.right() > screen.right()) {
                popupRect.moveRight(screen.right());
            }
            if (popupRect.top() < screen.top() || popupRect.bottom() > screen.bottom()) {
                auto flippedPopupRect =
                    QRectF(popupOffset(cursorRectangle, Qt::TopEdge | Qt::LeftEdge, Qt::RightEdge | Qt::TopEdge, surface()->size()), surface()->size());

                // if it still doesn't fit we should continue with the unflipped version
                if (flippedPopupRect.top() >= screen.top() || flippedPopupRect.bottom() <= screen.bottom()) {
                    popupRect.moveTop(flippedPopupRect.top());
                }
            }
            moveResize(popupRect);
        }
    } break;
    }
}

void InputPanelV1Window::destroyWindow()
{
    markAsZombie();

    Deleted *deleted = Deleted::create(this);
    Q_EMIT windowClosed(this, deleted);
    StackingUpdatesBlocker blocker(workspace());
    waylandServer()->removeWindow(this);
    deleted->unrefWindow();

    delete this;
}

NET::WindowType InputPanelV1Window::windowType(bool, int) const
{
    return NET::Utility;
}

QRectF InputPanelV1Window::inputGeometry() const
{
    return readyForPainting() ? QRectF(surface()->input().boundingRect()).translated(pos()) : QRectF();
}

void InputPanelV1Window::moveResizeInternal(const QRectF &rect, MoveResizeMode mode)
{
    updateGeometry(rect);
}

void InputPanelV1Window::handleMapped()
{
    updateDepth();
    maybeShow();
}

void InputPanelV1Window::maybeShow()
{
    const bool shouldShow = m_mode == Mode::Overlay || (m_mode == Mode::VirtualKeyboard && m_allowed && m_virtualKeyboardShouldBeShown);
    if (shouldShow && !isZombie() && surface()->isMapped()) {
        setReadyForPainting();
        reposition();
        showClient();
    }
}

} // namespace KWin

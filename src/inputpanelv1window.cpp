/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpanelv1window.h"
#include "deleted.h"
#include "inputmethod.h"
#include "output.h"
#include "platform.h"
#include "wayland/output_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
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
    connect(surface(), &SurfaceInterface::mapped, this, &InputPanelV1Window::updateDepth);

    connect(panelSurface, &InputPanelSurfaceV1Interface::topLevel, this, &InputPanelV1Window::showTopLevel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::overlayPanel, this, &InputPanelV1Window::showOverlayPanel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::destroyed, this, &InputPanelV1Window::destroyWindow);

    InputMethod::self()->setPanel(this);
}

void InputPanelV1Window::showOverlayPanel()
{
    setOutput(nullptr);
    m_mode = Overlay;
    reposition();
    showClient();
    setReadyForPainting();
}

void InputPanelV1Window::showTopLevel(OutputInterface *output, InputPanelSurfaceV1Interface::Position position)
{
    Q_UNUSED(position);
    m_mode = Toplevel;
    setOutput(output);
    showClient();
}

void InputPanelV1Window::allow()
{
    setReadyForPainting();
    reposition();
}

void KWin::InputPanelV1Window::reposition()
{
    if (!readyForPainting()) {
        return;
    }

    switch (m_mode) {
    case Toplevel: {
        QSize panelSize = surface()->size();
        if (!panelSize.isValid() || panelSize.isEmpty()) {
            return;
        }

        QRect availableArea;
        QRect outputArea;
        if (m_output) {
            outputArea = m_output->geometry();
            if (waylandServer()->isScreenLocked()) {
                availableArea = outputArea;
            } else {
                availableArea = workspace()->clientArea(MaximizeArea, this, m_output);
            }
        } else {
            availableArea = workspace()->clientArea(MaximizeArea, this);
            outputArea = workspace()->clientArea(FullScreenArea, this);
        }

        panelSize = panelSize.boundedTo(availableArea.size());

        QRect geo(availableArea.bottomLeft() - QPoint{0, panelSize.height()}, panelSize);
        geo.translate((availableArea.width() - panelSize.width()) / 2, availableArea.height() - outputArea.height());
        moveResize(geo);
    } break;
    case Overlay: {
        auto textInputSurface = waylandServer()->seat()->focusedTextInputSurface();
        auto textWindow = waylandServer()->findWindow(textInputSurface);
        QRect cursorRectangle;
        auto textInputV2 = waylandServer()->seat()->textInputV2();
        if (textInputV2 && textInputV2->isEnabled() && textInputV2->surface() == textInputSurface) {
            cursorRectangle = textInputV2->cursorRectangle();
        }
        auto textInputV3 = waylandServer()->seat()->textInputV3();
        if (textInputV3 && textInputV3->isEnabled() && textInputV3->surface() == textInputSurface) {
            cursorRectangle = textInputV3->cursorRectangle();
        }
        if (textWindow) {
            cursorRectangle.translate(textWindow->bufferGeometry().topLeft());
            const QRect screen = Workspace::self()->clientArea(PlacementArea, this, cursorRectangle.bottomLeft());

            // Reuse the similar logic like xdg popup
            QRect popupRect(popupOffset(cursorRectangle, Qt::BottomEdge | Qt::LeftEdge, Qt::RightEdge | Qt::BottomEdge, surface()->size()), surface()->size());

            if (popupRect.left() < screen.left()) {
                popupRect.moveLeft(screen.left());
            }
            if (popupRect.right() > screen.right()) {
                popupRect.moveRight(screen.right());
            }
            if (popupRect.top() < screen.top() || popupRect.bottom() > screen.bottom()) {
                auto flippedPopupRect =
                    QRect(popupOffset(cursorRectangle, Qt::TopEdge | Qt::LeftEdge, Qt::RightEdge | Qt::TopEdge, surface()->size()), surface()->size());

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

QRect InputPanelV1Window::inputGeometry() const
{
    return readyForPainting() ? surface()->input().boundingRect().translated(pos()) : QRect();
}

void InputPanelV1Window::setOutput(OutputInterface *outputIface)
{
    if (m_output) {
        disconnect(m_output, &Output::geometryChanged, this, &InputPanelV1Window::reposition);
    }

    m_output = waylandServer()->findOutput(outputIface);

    if (m_output) {
        connect(m_output, &Output::geometryChanged, this, &InputPanelV1Window::reposition);
    }
}

void InputPanelV1Window::moveResizeInternal(const QRect &rect, MoveResizeMode mode)
{
    Q_UNUSED(mode)
    updateGeometry(rect);
}

} // namespace KWin

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputpanelv1client.h"
#include "deleted.h"
#include "wayland_server.h"
#include "workspace.h"
#include "abstract_wayland_output.h"
#include "platform.h"
#include <KWaylandServer/output_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/textinput_interface.h>

using namespace KWin;
using namespace KWaylandServer;

InputPanelV1Client::InputPanelV1Client(InputPanelSurfaceV1Interface *panelSurface)
    : WaylandClient(panelSurface->surface())
    , m_panelSurface(panelSurface)
{
    setSkipPager(true);
    setSkipTaskbar(true);
    setKeepAbove(true);
    setupCompositing();

    connect(surface(), &SurfaceInterface::aboutToBeDestroyed, this, &InputPanelV1Client::destroyClient);
    connect(surface(), &SurfaceInterface::sizeChanged, this, &InputPanelV1Client::reposition);
    connect(surface(), &SurfaceInterface::mapped, this, &InputPanelV1Client::updateDepth);
    connect(surface(), &SurfaceInterface::damaged, this, QOverload<const QRegion &>::of(&WaylandClient::addRepaint));

    connect(panelSurface, &InputPanelSurfaceV1Interface::topLevel, this, &InputPanelV1Client::showTopLevel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::overlayPanel, this, &InputPanelV1Client::showOverlayPanel);
    connect(panelSurface, &InputPanelSurfaceV1Interface::destroyed, this, &InputPanelV1Client::destroyClient);
}

void InputPanelV1Client::showOverlayPanel()
{
    setOutput(nullptr);
    m_mode = Overlay;
    reposition();
    setReadyForPainting();
}

void InputPanelV1Client::showTopLevel(OutputInterface *output, InputPanelSurfaceV1Interface::Position position)
{
    Q_UNUSED(position);
    m_mode = Toplevel;
    setOutput(output);
    reposition();
    setReadyForPainting();
}

void KWin::InputPanelV1Client::reposition()
{
    switch (m_mode) {
        case Toplevel: {
            if (m_output) {
                const QSize panelSize = surface()->size();
                if (!panelSize.isValid() || panelSize.isEmpty()) {
                    return;
                }

                const auto outputGeometry = m_output->geometry();
                QRect geo(outputGeometry.topLeft(), panelSize);
                geo.translate((outputGeometry.width() - panelSize.width())/2, outputGeometry.height() - panelSize.height());
                setFrameGeometry(geo);
            }
        }   break;
        case Overlay: {
            auto textClient = waylandServer()->findClient(waylandServer()->seat()->focusedTextInputSurface());
            auto textInput = waylandServer()->seat()->focusedTextInput();
            if (textClient && textInput) {
                const auto cursorRectangle = textInput->cursorRectangle();
                setFrameGeometry({textClient->pos() + textClient->clientPos() + cursorRectangle.bottomLeft(), surface()->size()});
            }
        }   break;
    }
}

void InputPanelV1Client::setFrameGeometry(const QRect &geometry, ForceGeometry_t force)
{
    Q_UNUSED(force);
    if (m_frameGeometry != geometry) {
        const QRect oldFrameGeometry = m_frameGeometry;
        m_frameGeometry = geometry;
        m_clientGeometry = geometry;

        emit frameGeometryChanged(this, oldFrameGeometry);
        emit clientGeometryChanged(this, oldFrameGeometry);
        emit bufferGeometryChanged(this, oldFrameGeometry);
        emit geometryShapeChanged(this, oldFrameGeometry);

        addRepaintDuringGeometryUpdates();
    }
}

void InputPanelV1Client::destroyClient()
{
    markAsZombie();

    Deleted *deleted = Deleted::create(this);
    emit windowClosed(this, deleted);
    StackingUpdatesBlocker blocker(workspace());
    waylandServer()->removeClient(this);
    deleted->unrefWindow();

    delete this;
}

void InputPanelV1Client::debug(QDebug &stream) const
{
    stream << "InputPanelClient(" << static_cast<const void*>(this) << "," << resourceClass() << m_frameGeometry << ')';
}

NET::WindowType InputPanelV1Client::windowType(bool, int) const
{
    return NET::Utility;
}

QRect InputPanelV1Client::inputGeometry() const
{
    return surface()->input().boundingRect().translated(pos());
}

void InputPanelV1Client::hideClient(bool hide)
{
    m_visible = !hide;
    if (hide) {
        workspace()->clientHidden(this);
        addWorkspaceRepaint(visibleRect());
        Q_EMIT windowHidden(this);
    } else {
        reposition();
        addRepaintFull();
        Q_EMIT windowShown(this);
        autoRaise();
    }
}

void InputPanelV1Client::setOutput(OutputInterface *outputIface)
{
    if (m_output) {
        disconnect(m_output, &AbstractWaylandOutput::geometryChanged, this, &InputPanelV1Client::reposition);
    }

    m_output = waylandServer()->findOutput(outputIface);

    if (m_output) {
        connect(m_output, &AbstractWaylandOutput::geometryChanged, this, &InputPanelV1Client::reposition);
    }
}

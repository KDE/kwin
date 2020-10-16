/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandclient.h"
#include <QPointer>
#include <KWaylandServer/inputmethod_v1_interface.h>

namespace KWin
{
class AbstractWaylandOutput;

class InputPanelV1Client : public WaylandClient
{
    Q_OBJECT
public:
    InputPanelV1Client(KWaylandServer::InputPanelSurfaceV1Interface *panelSurface);

    enum Mode {
        Toplevel,
        Overlay,
    };
    Q_ENUM(Mode)

    void destroyClient() override;
    bool isPlaceable() const override { return false; }
    bool isCloseable() const override { return false; }
    bool isResizable() const override { return false; }
    bool isMovable() const override { return false; }
    bool isMovableAcrossScreens() const override { return false; }
    bool acceptsFocus() const override { return false; }
    void closeWindow() override {}
    bool takeFocus() override { return false; }
    bool wantsInput() const override { return false; }
    bool isInputMethod() const override { return true; }
    bool isInitialPositionSet() const override { return true; }
    NET::WindowType windowType(bool /*direct*/, int /*supported_types*/) const override;
    QRect inputGeometry() const override;

private:
    void showTopLevel(KWaylandServer::OutputInterface *output, KWaylandServer::InputPanelSurfaceV1Interface::Position position);
    void showOverlayPanel();
    void reposition();
    void setOutput(KWaylandServer::OutputInterface* output);

    QPointer<AbstractWaylandOutput> m_output;
    Mode m_mode = Toplevel;
    const QPointer<KWaylandServer::InputPanelSurfaceV1Interface> m_panelSurface;
};

}

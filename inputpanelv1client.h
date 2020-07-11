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

    void setFrameGeometry(const QRect &geometry, KWin::AbstractClient::ForceGeometry_t force = NormalGeometrySet) override;

    void destroyClient() override;
    QRect bufferGeometry() const override { return frameGeometry(); }
    bool isCloseable() const override { return false; }
    bool noBorder() const override { return true; }
    bool isResizable() const override { return false; }
    bool isMovable() const override { return false; }
    bool isMovableAcrossScreens() const override { return false; }
    bool userCanSetNoBorder() const override { return false; }
    bool acceptsFocus() const override { return false; }
    void showOnScreenEdge() override {}
    bool supportsWindowRules() const override { return false; }
    void closeWindow() override {}
    void hideClient(bool hide) override;
    bool isHiddenInternal() const override { return !m_visible; }
    bool takeFocus() override { return false; }
    void updateColorScheme() override {}
    bool wantsInput() const override { return false; }
    bool isInputMethod() const override { return true; }
    bool isShown(bool /*shaded_is_shown*/) const override { return m_visible && !isZombie(); }
    bool isInitialPositionSet() const override { return true; }
    void updateDecoration(bool /*check_workspace_pos*/, bool /*force*/) override {}
    void setNoBorder(bool /*set*/) override {}
    NET::WindowType windowType(bool /*direct*/, int /*supported_types*/) const override;
    void debug(QDebug & stream) const override;

private:
    void showTopLevel(KWaylandServer::OutputInterface *output, KWaylandServer::InputPanelSurfaceV1Interface::Position position);
    void showOverlayPanel();
    void reposition();
    void setOutput(KWaylandServer::OutputInterface* output);

    QPointer<AbstractWaylandOutput> m_output;
    Mode m_mode = Toplevel;
    const QPointer<KWaylandServer::InputPanelSurfaceV1Interface> m_panelSurface;
    bool m_visible = true;
};

}

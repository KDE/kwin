/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wayland/inputmethod_v1_interface.h"
#include "waylandwindow.h"
#include <QPointer>

namespace KWin
{
class Output;

class InputPanelV1Window : public WaylandWindow
{
    Q_OBJECT
public:
    InputPanelV1Window(KWaylandServer::InputPanelSurfaceV1Interface *panelSurface);

    enum class Mode {
        None,
        VirtualKeyboard,
        Overlay,
    };
    Q_ENUM(Mode)

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
    bool takeFocus() override
    {
        return false;
    }
    bool wantsInput() const override
    {
        return false;
    }
    bool isInputMethod() const override
    {
        return true;
    }
    NET::WindowType windowType(bool /*direct*/, int /*supported_types*/) const override;
    QRectF inputGeometry() const override;

    Mode mode() const
    {
        return m_mode;
    }
    void allow();
    void show();
    void hide();

protected:
    void moveResizeInternal(const QRectF &rect, MoveResizeMode mode) override;

private:
    void showTopLevel(KWaylandServer::OutputInterface *output, KWaylandServer::InputPanelSurfaceV1Interface::Position position);
    void showOverlayPanel();
    void reposition();
    void handleMapped();
    void maybeShow();

    Mode m_mode = Mode::None;
    bool m_allowed = false;
    bool m_virtualKeyboardShouldBeShown = false;
    const QPointer<KWaylandServer::InputPanelSurfaceV1Interface> m_panelSurface;
};

}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wayland/inputmethod_v1.h"
#include "waylandwindow.h"
#include <QPointer>

namespace KWin
{
class Output;

class InputPanelV1Window : public WaylandWindow
{
    Q_OBJECT
public:
    InputPanelV1Window(InputPanelSurfaceV1Interface *panelSurface);

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
    WindowType windowType() const override;
    QRectF frameRectToBufferRect(const QRectF &rect) const override;

    Mode mode() const
    {
        return m_mode;
    }
    void allow();
    void show();
    void hide();

protected:
    void moveResizeInternal(const QRectF &rect, MoveResizeMode mode) override;
    void doSetNextTargetScale() override;
    void doSetPreferredBufferTransform() override;
    void doSetPreferredColorDescription() override;

private:
    void showTopLevel(OutputInterface *output, InputPanelSurfaceV1Interface::Position position);
    void showOverlayPanel();
    void resetPosition();
    void reposition();
    void handleMapped();
    void maybeShow();

    QRectF m_windowGeometry;
    Mode m_mode = Mode::None;
    bool m_allowed = false;
    bool m_virtualKeyboardShouldBeShown = false;
    const QPointer<InputPanelSurfaceV1Interface> m_panelSurface;
    QTimer m_rescalingTimer;
};

}

/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wayland/xxpip_v1.h"
#include "xdgshellwindow.h"

namespace KWin
{

class XXPipV1Window final : public XdgSurfaceWindow
{
    Q_OBJECT

public:
    explicit XXPipV1Window(XXPipV1Interface *shellSurface);

    bool isPictureInPicture() const override;
    bool isResizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isCloseable() const override;
    void closeWindow() override;
    bool wantsInput() const override;
    bool takeFocus() override;

protected:
    bool acceptsFocus() const override;
    XdgSurfaceConfigure *sendRoleConfigure() const override;
    void handleRoleDestroyed() override;
    void doSetNextTargetScale() override;
    void doSetPreferredBufferTransform() override;
    void doSetPreferredColorDescription() override;

private:
    void initialize();
    void handleApplicationIdChanged();
    void handleMoveRequested(SeatInterface *seat, quint32 serial);
    void handleResizeRequested(SeatInterface *seat, Gravity gravity, quint32 serial);

    XXPipV1Interface *m_shellSurface;
};

} // namespace KWin

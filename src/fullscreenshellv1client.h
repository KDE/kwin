/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandclient.h"
#include <KWaylandServer/fullscreenshell_v1_interface.h>
#include <QPointer>

namespace KWin
{
class AbstractWaylandOutput;

class FullscreenShellV1Client : public WaylandClient
{
    Q_OBJECT
public:
    FullscreenShellV1Client(KWaylandServer::SurfaceInterface *surface);

    bool isPlaceable() const override
    {
        return false;
    }
    bool isCloseable() const override
    {
        return true;
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
        return true;
    }
    void closeWindow() override
    {
    }
    bool takeFocus() override;
    bool wantsInput() const override
    {
        return true;
    }
    bool isInputMethod() const override
    {
        return false;
    }

    void destroyClient() override;
    NET::WindowType windowType(bool direct, int supportedTypes) const override;

    void setPresentMethod(KWaylandServer::FullscreenShellV1Interface::PresentMethod method);
    void setOutput(KWaylandServer::OutputInterface *output);

private:
    void reposition();

    QPointer<AbstractWaylandOutput> m_output;
    KWaylandServer::FullscreenShellV1Interface::PresentMethod m_method;
};

}

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010, 2011, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/effecthandler.h"
#include "utils/xcbutils.h"

#include <memory.h>

namespace KWin
{
class EffectsMouseInterceptionX11Filter;
class EffectsKeyboardInterceptionX11Filter;

class EffectsHandlerX11 : public EffectsHandler
{
    Q_OBJECT
public:
    explicit EffectsHandlerX11(Compositor *compositor, WorkspaceScene *scene);
    ~EffectsHandlerX11() override;

    void defineCursor(Qt::CursorShape shape) override;

protected:
    bool doGrabKeyboard() override;
    void doUngrabKeyboard() override;

    void doStartMouseInterception(Qt::CursorShape shape) override;
    void doStopMouseInterception() override;

    void doCheckInputWindowStacking() override;

private:
    Xcb::Window m_mouseInterceptionWindow;
    std::unique_ptr<EffectsMouseInterceptionX11Filter> m_x11MouseInterception;
    std::unique_ptr<EffectsKeyboardInterceptionX11Filter> m_x11KeyboardInterception;
};

}

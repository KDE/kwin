/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "compositor.h"
#include "effect/globals.h"

namespace KWin
{

class KWIN_EXPORT WaylandCompositor final : public Compositor
{
    Q_OBJECT
public:
    static WaylandCompositor *create(QObject *parent = nullptr);
    ~WaylandCompositor() override;

    void createRenderer();

    void start() override;
    void stop() override;

protected:
    void composite(RenderLoop *loop) override;

private:
    explicit WaylandCompositor(QObject *parent);
    void createScene();

    bool attemptOpenGLCompositing();
    bool attemptQPainterCompositing();

    void addOutput(Output *output);
    void removeOutput(Output *output);

    CompositingType m_selectedCompositor = NoCompositing;
};

} // namespace KWin

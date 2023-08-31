/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "compositor.h"

namespace KWin
{

class KWIN_EXPORT WaylandCompositor final : public Compositor
{
    Q_OBJECT
public:
    static WaylandCompositor *create(QObject *parent = nullptr);
    ~WaylandCompositor() override;

    void toggleCompositing() override;

protected:
    void start() override;
    void composite(RenderLoop *loop) override;

private:
    explicit WaylandCompositor(QObject *parent);

    void prePaintPass(RenderLayer *layer);
    void postPaintPass(RenderLayer *layer);
    void preparePaintPass(RenderLayer *layer, QRegion *repaint);
    void paintPass(RenderLayer *layer, const RenderTarget &renderTarget, const QRegion &region);
    void framePass(RenderLayer *layer);
};

} // namespace KWin

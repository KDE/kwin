/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screencastsource.h"
#include "window.h"

#include <QPointer>
#include <QTimer>

namespace KWin
{

class WindowScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit WindowScreenCastSource(Window *window, QObject *parent = nullptr);

    bool hasAlphaChannel() const override;
    QSize textureSize() const override;
    uint refreshRate() const override;

    void render(GLFramebuffer *target) override;
    void render(spa_data *spa, spa_video_format format) override;
    std::chrono::nanoseconds clock() const override;

private:
    QPointer<Window> m_window;
    WindowOffscreenRenderRef m_offscreenRef;
};

} // namespace KWin

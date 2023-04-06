/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screencastsource.h"

#include <QPointer>

namespace KWin
{

class Output;

class OutputScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit OutputScreenCastSource(Output *output, QObject *parent = nullptr);

    uint refreshRate() const override;
    bool hasAlphaChannel() const override;
    QSize textureSize() const override;

    void render(GLFramebuffer *target) override;
    void render(spa_data *spa, spa_video_format format) override;
    std::chrono::nanoseconds clock() const override;

private:
    QPointer<Output> m_output;
};

} // namespace KWin

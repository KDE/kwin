/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <spa/buffer/buffer.h>
#include <spa/param/video/raw.h>

namespace KWin
{

class GLFramebuffer;

class ScreenCastSource : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCastSource(QObject *parent = nullptr);

    virtual uint refreshRate() const = 0;
    virtual bool hasAlphaChannel() const = 0;
    virtual QSize textureSize() const = 0;

    virtual void render(GLFramebuffer *target) = 0;
    virtual void render(spa_data *spa, spa_video_format format) = 0;
    virtual std::chrono::nanoseconds clock() const = 0;

Q_SIGNALS:
    void closed();
};

} // namespace KWin

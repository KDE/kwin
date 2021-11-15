/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

namespace KWin
{

class GLRenderTarget;

class ScreenCastSource : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCastSource(QObject *parent = nullptr);

    virtual bool hasAlphaChannel() const = 0;
    virtual QSize textureSize() const = 0;

    virtual void render(GLRenderTarget *target) = 0;
    virtual void render(QImage *image) = 0;

Q_SIGNALS:
    void closed();
};

} // namespace KWin

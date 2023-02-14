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

    bool hasAlphaChannel() const override;
    QSize textureSize() const override;
    quint32 drmFormat() const override;

    void render(GLFramebuffer *target) override;
    void render(QImage *image) override;
    std::chrono::nanoseconds clock() const override;

private:
    QPointer<Output> m_output;
};

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screencastsource.h"

#include <QPointer>

namespace KWin
{

class AbstractOutput;

class OutputScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit OutputScreenCastSource(AbstractOutput *output, QObject *parent = nullptr);

    bool hasAlphaChannel() const override;
    QSize textureSize() const override;

    void render(GLRenderTarget *target) override;
    void render(QImage *image) override;

private:
    QPointer<AbstractOutput> m_output;
};

} // namespace KWin

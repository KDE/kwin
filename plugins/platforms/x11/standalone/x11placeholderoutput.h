/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "abstract_output.h"

namespace KWin
{

class X11PlaceholderOutput : public AbstractOutput
{
    Q_OBJECT

public:
    explicit X11PlaceholderOutput(QObject *parent = nullptr);

    QString name() const override;
    QRect geometry() const override;
    int refreshRate() const override;
    QSize pixelSize() const override;
};

} // namespace KWin

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
    explicit X11PlaceholderOutput(RenderLoop *loop, QObject *parent = nullptr);

    RenderLoop *renderLoop() const override;

private:
    RenderLoop *m_loop;
};

} // namespace KWin

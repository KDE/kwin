/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "output.h"

namespace KWin
{

class X11StandalonePlatform;

class X11PlaceholderOutput : public Output
{
    Q_OBJECT

public:
    explicit X11PlaceholderOutput(X11StandalonePlatform *backend, QObject *parent = nullptr);

    RenderLoop *renderLoop() const override;

private:
    X11StandalonePlatform *m_backend;
};

} // namespace KWin

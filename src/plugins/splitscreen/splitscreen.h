/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "plugin.h"

namespace KWin
{
class Output;
class ScreenCastStream;

class Splitscreen : public Plugin
{
    Q_OBJECT

public:
    explicit Splitscreen(QObject *parent = nullptr);

    void splitActiveOutput();
};

} // namespace KWin

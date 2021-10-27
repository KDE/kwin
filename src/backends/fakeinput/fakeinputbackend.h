/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "inputbackend.h"

namespace KWin
{

class FakeInputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit FakeInputBackend(QObject *parent = nullptr);

    void initialize() override;
};

} // namespace KWin

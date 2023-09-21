/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/inputbackend.h"

#include <map>
#include <memory>

namespace KWin
{

class Display;
class FakeInputBackendPrivate;
class FakeInputDevice;

class FakeInputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit FakeInputBackend(Display *display);
    ~FakeInputBackend();

    void initialize() override;

private:
    std::unique_ptr<FakeInputBackendPrivate> d;
};

} // namespace KWin

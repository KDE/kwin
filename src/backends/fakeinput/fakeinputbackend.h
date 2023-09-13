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

class FakeInputDevice;
class FakeInputInterface;
class FakeInputDeviceInterface;

class FakeInputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit FakeInputBackend();
    ~FakeInputBackend();

    void initialize() override;

private:
    std::unique_ptr<FakeInputInterface> m_interface;
    std::map<FakeInputDeviceInterface *, std::unique_ptr<FakeInputDevice>> m_devices;
};

} // namespace KWin

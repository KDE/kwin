/*
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>

class QQmlEngine;

namespace KWin
{

class SharedQmlEngine
{
public:
    using Ptr = std::shared_ptr<QQmlEngine>;
    static Ptr engine();

private:
    static std::weak_ptr<QQmlEngine> s_engine;
};

} // namespace KWin

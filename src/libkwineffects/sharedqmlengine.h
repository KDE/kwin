/*
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSharedPointer>

class QQmlEngine;

namespace KWin
{

class SharedQmlEngine
{
public:
    using Ptr = QSharedPointer<QQmlEngine>;
    static Ptr engine();

private:
    static QWeakPointer<QQmlEngine> s_engine;
};

} // namespace KWin

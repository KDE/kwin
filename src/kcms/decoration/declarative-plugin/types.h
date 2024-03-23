/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QQmlEngine>

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationShadow>

struct DecorationForeign
{
    Q_GADGET
    QML_ANONYMOUS
    QML_FOREIGN(KDecoration2::Decoration)
};

struct DecorationShadowForeign
{
    Q_GADGET
    QML_ANONYMOUS
    QML_FOREIGN(KDecoration2::DecorationShadow)
};

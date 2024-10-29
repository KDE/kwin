/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QQmlEngine>

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationShadow>

struct DecorationForeign
{
    Q_GADGET
    QML_ANONYMOUS
    QML_FOREIGN(KDecoration3::Decoration)
};

struct DecorationShadowForeign
{
    Q_GADGET
    QML_ANONYMOUS
    QML_FOREIGN(KDecoration3::DecorationShadow)
};

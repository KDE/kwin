/*
    SPDX-FileCopyrightText: 2016 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#pragma once

#include <QFlags>
#include <QString>

namespace KWin
{
namespace OSD
{

void show(const QString &message, const QString &iconName = QString());
void show(const QString &message, int timeout);
void show(const QString &message, const QString &iconName, int timeout);
enum class HideFlag {
    SkipCloseAnimation = 1,
};
Q_DECLARE_FLAGS(HideFlags, HideFlag)
void hide(HideFlags flags = HideFlags());

}
}

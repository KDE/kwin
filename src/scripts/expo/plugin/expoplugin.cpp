/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "expoplugin.h"
#include "expolayout.h"

#include <QQmlEngine>

void ExpoPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<ExpoCell>(uri, 1, 0, "ExpoCell");
    qmlRegisterType<ExpoLayout>(uri, 1, 0, "ExpoLayout");
}

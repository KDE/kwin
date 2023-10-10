/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugin.h"
#include "expoarea.h"
#include "expolayout.h"
#include "libkwineffects/effecttogglablestate.h"

void EffectKitExtensionPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<KWin::ExpoArea>(uri, 1, 0, "ExpoArea");
    qmlRegisterType<ExpoLayout>(uri, 1, 0, "ExpoLayout");
    qmlRegisterType<ExpoCell>(uri, 1, 0, "ExpoCell");
    qmlRegisterType<KWin::EffectTogglableGesture>(uri, 1, 0, "EffectTogglableGesture");
    qmlRegisterType<KWin::EffectTogglableState>(uri, 1, 0, "EffectTogglableState");
}

#include "moc_plugin.cpp"

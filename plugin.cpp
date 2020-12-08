/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugin.h"

namespace KWin
{

Plugin::Plugin(QObject *parent)
    : QObject(parent)
{
}

PluginFactory::PluginFactory(QObject *parent)
    : QObject(parent)
{
}

} // namespace KWin

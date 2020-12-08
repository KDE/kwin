/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_effectshandler.h"

MockEffectsHandler::MockEffectsHandler(KWin::CompositingType type)
    : EffectsHandler(type)
{
}


KSharedConfigPtr MockEffectsHandler::config() const
{
    static const KSharedConfigPtr s_config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    return s_config;
}

KSharedConfigPtr MockEffectsHandler::inputConfig() const
{
    static const KSharedConfigPtr s_inputConfig = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    return s_inputConfig;
}

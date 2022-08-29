/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputbackend.h"

namespace KWin
{

InputBackend::InputBackend(QObject *parent)
    : QObject(parent)
{
}

KSharedConfigPtr InputBackend::config() const
{
    return m_config;
}

void InputBackend::setConfig(KSharedConfigPtr config)
{
    m_config = config;
}

} // namespace KWin

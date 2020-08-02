/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <cstddef>
#include <cstdint>
#include <xf86drmMode.h>

#include <QVector>

namespace MockDrm
{

void addDrmModeProperties(int fd, const QVector<_drmModeProperty> &properties);

}

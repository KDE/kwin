/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>

class QTemporaryFile;

bool generateXauthorityFile(const QList<int> &displays, QTemporaryFile *authorityFile);

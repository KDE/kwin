/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/inputdevice.h"

#include <kglobalaccel_interface.h>

#include <QObject>

class KGlobalAccelImpl : public KGlobalAccelInterface
{
    Q_OBJECT

public:
    KGlobalAccelImpl();
    ~KGlobalAccelImpl() override;

public Q_SLOTS:
    bool checkKeyPressed(int keyQt, KWin::KeyboardKeyState state);
    bool checkPointerPressed(Qt::MouseButtons buttons);
    bool checkAxisTriggered(int axis);
    void cancelModiferOnlySequence();
};

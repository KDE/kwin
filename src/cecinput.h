/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

namespace CEC {
    class ICECAdapter;
}

/**
 * Adds support for CEC (Consumer Electronics Control) input systems
 *
 * This is generally used on embedded systems that are connected to a TV so
 * that you can use the TV's remote as an input device.
 */
class CECInput : public QObject
{
    Q_OBJECT
public:
    CECInput(QObject *parent);
    ~CECInput();

private:
    bool m_opened = false;
    CEC::ICECAdapter *m_cecAdapter = nullptr;
};

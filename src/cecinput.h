/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QThread>

#include <libcec/cec.h>

namespace CEC {
    class ICECAdapter;
}

/**
 * Adds support for CEC (Consumer Electronics Control) input systems
 *
 * This is generally used on embedded systems that are connected to a TV so
 * that you can use the TV's remote as an input device.
 */
class CECInput : public QThread
{
    Q_OBJECT
public:
    CECInput(QObject *parent);
    ~CECInput();

    void run() override;

private:
    bool m_opened = false;
    CEC::ICECAdapter *m_cecAdapter = nullptr;
    CEC::ICECCallbacks m_cecCallbacks;
    static QHash<int, int> m_keyCodeTranslation;

    static void handleCecKeypress(void* data, const CEC::cec_keypress* key);
    static void handleCecLogMessage(void* data, const CEC::cec_log_message* key);
};

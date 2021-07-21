/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cecinput.h"
#include <QDebug>

#include <iostream> //???
#include <libcec/cec.h>
#include <libcec/cecloader.h>
#include <libcec/cectypes.h>

#include <linux/input.h>
#include "platform.h"
#include "main.h"
#include "cec_logging.h"

using namespace CEC;

void handleCecKeypress(void* data, const CEC::cec_keypress* key)
{
    Q_UNUSED(data);
    static const QHash<int, int> keyCodeTranslation = {
        { CEC::CEC_USER_CONTROL_CODE_PLAY, KEY_PLAY},
        { CEC::CEC_USER_CONTROL_CODE_STOP, KEY_STOP},
        { CEC::CEC_USER_CONTROL_CODE_REWIND, KEY_REWIND},
        { CEC::CEC_USER_CONTROL_CODE_FAST_FORWARD, KEY_FASTFORWARD},
        { CEC::CEC_USER_CONTROL_CODE_SELECT, KEY_SELECT},
        { CEC::CEC_USER_CONTROL_CODE_UP, KEY_UP},
        { CEC::CEC_USER_CONTROL_CODE_DOWN, KEY_DOWN},
        { CEC::CEC_USER_CONTROL_CODE_LEFT, KEY_LEFT},
        { CEC::CEC_USER_CONTROL_CODE_RIGHT, KEY_RIGHT},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER0, KEY_0},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER1, KEY_1},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER2, KEY_2},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER3, KEY_3},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER4, KEY_4},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER5, KEY_5},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER6, KEY_6},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER7, KEY_7},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER8, KEY_8},
        { CEC::CEC_USER_CONTROL_CODE_NUMBER9, KEY_9},
        { CEC::CEC_USER_CONTROL_CODE_F1_BLUE, KEY_BLUE},
        { CEC::CEC_USER_CONTROL_CODE_F2_RED, KEY_RED},
        { CEC::CEC_USER_CONTROL_CODE_F3_GREEN, KEY_GREEN},
        { CEC::CEC_USER_CONTROL_CODE_F4_YELLOW, KEY_YELLOW},
        { CEC::CEC_USER_CONTROL_CODE_CHANNEL_UP, KEY_CHANNELUP},
        { CEC::CEC_USER_CONTROL_CODE_CHANNEL_DOWN, KEY_CHANNELDOWN},
        { CEC::CEC_USER_CONTROL_CODE_EXIT, KEY_EXIT},
        { CEC::CEC_USER_CONTROL_CODE_AN_RETURN, KEY_BACK},
        { CEC::CEC_USER_CONTROL_CODE_ROOT_MENU, KEY_HOME},
        { CEC::CEC_USER_CONTROL_CODE_SUB_PICTURE, KEY_SUBTITLE},
        { CEC::CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION, KEY_INFO},
    };
    int nativeKeyCode = keyCodeTranslation.value(key->keycode, -1);

    if (nativeKeyCode != -1) {
        if (key->duration) {
            KWin::kwinApp()->platform()->keyboardKeyReleased(nativeKeyCode, QDateTime::currentMSecsSinceEpoch());
        } else {
            KWin::kwinApp()->platform()->keyboardKeyPressed(nativeKeyCode, QDateTime::currentMSecsSinceEpoch());
        }
    } else {
        qWarning() << "Could not interpret CEC keycode:" << key->keycode;
    }
}


void handleCecLogMessage(void *param, const cec_log_message* message)
{
    Q_UNUSED(param);

    std::string strLevel;
    switch (message->level)
    {
    case CEC_LOG_ERROR:
        qCCritical(KWIN_CEC) << message->message;
        break;
    case CEC_LOG_WARNING:
        qCWarning(KWIN_CEC) << message->message;
        break;
    case CEC_LOG_NOTICE:
    case CEC_LOG_TRAFFIC:
        qCInfo(KWIN_CEC) << message->message;
        break;
        break;
    case CEC_LOG_DEBUG:
        qCDebug(KWIN_CEC) << message->message;
        break;
    default:
        break;
    }
}

void handleCecKeyPress(void *param, const cec_keypress* key)
{
    Q_UNUSED(param)
    Q_UNUSED(key)
}

void handleCecCommand(void *param, const cec_command* command)
{
    Q_UNUSED(param)
    Q_UNUSED(command)
}

void handleCecAlert(void *param, const libcec_alert type, const libcec_parameter cecparam)
{
    Q_UNUSED(param)
    Q_UNUSED(cecparam)
    switch (type)
    {
    case CEC_ALERT_CONNECTION_LOST:
        qCWarning(KWIN_CEC) << "Connection lost!";
        break;
    default:
        break;
    }
}

CECInput::CECInput(QObject *p)
    : QObject(p)
{
    CEC::ICECCallbacks cec_callbacks;
    cec_callbacks.Clear();
    cec_callbacks.keyPress        = &handleCecKeypress;
    cec_callbacks.logMessage      = &handleCecLogMessage;
    cec_callbacks.keyPress        = &handleCecKeyPress;
    cec_callbacks.commandReceived = &handleCecCommand;
    cec_callbacks.alert           = &handleCecAlert;

    CEC::libcec_configuration cec_config;
    cec_config.clientVersion = LIBCEC_VERSION_CURRENT;
    cec_config.callbacks = &cec_callbacks;
    cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_RECORDING_DEVICE);

    m_cecAdapter = LibCecInitialise(&cec_config);

    if(!m_cecAdapter) {
        qDebug() << "Could not create CEC adaptor with current config";
        deleteLater();
        return;
    }

    CEC::cec_adapter_descriptor device;
    int devices_found = m_cecAdapter->DetectAdapters(&device, 1, NULL);
    if(devices_found < 1) {
        qWarning() << "No CEC devices detected";
        deleteLater();
        return;
    }

    if(!m_cecAdapter->Open(device.strComName)) {
        qWarning() << "Could not open CEC device" << device.strComPath << device.strComName;
        deleteLater();
        return;
    }
    m_opened = true;
}

CECInput::~CECInput() {
    if (m_cecAdapter) {
        if (m_opened) {
            m_cecAdapter->Close();
        }
        UnloadLibCec(m_cecAdapter);
        m_cecAdapter = nullptr;
    }
}

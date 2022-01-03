/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cecinput.h"
#include <QDebug>
#include <KSharedConfig>
#include <KConfigGroup>

#include <iostream> // libcec bug forces us to import this
#include <libcec/cec.h>
#include <libcec/cecloader.h>
#include <libcec/cectypes.h>

#include <linux/input.h>
#include "platform.h"
#include "main.h"
#include "cec_logging.h"

using namespace CEC;

QHash<int, int> CECInput::m_keyCodeTranslation;

void CECInput::handleCecKeypress(void* data, const cec_keypress* key)
{
    Q_UNUSED(data);
    int nativeKeyCode = m_keyCodeTranslation.value(key->keycode, -1);

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


void CECInput::handleCecLogMessage(void *data, const cec_log_message* message)
{
    Q_UNUSED(data);

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

CECInput::CECInput(QObject *p)
    : QThread(p)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup generalGroup = config->group("CECInput");

    m_keyCodeTranslation = {
        { generalGroup.readEntry("ButtonPlay", (int) CEC_USER_CONTROL_CODE_PLAY), KEY_PLAY},
        { generalGroup.readEntry("ButtonStop", (int) CEC_USER_CONTROL_CODE_STOP), KEY_STOP},
        { generalGroup.readEntry("ButtonPause", (int) CEC_USER_CONTROL_CODE_PAUSE), KEY_PAUSE},
        { generalGroup.readEntry("ButtonRewind", (int) CEC_USER_CONTROL_CODE_REWIND), KEY_REWIND},
        { generalGroup.readEntry("ButtonFastforward", (int) CEC_USER_CONTROL_CODE_FAST_FORWARD), KEY_FASTFORWARD},
        { generalGroup.readEntry("ButtonEnter", (int) CEC_USER_CONTROL_CODE_SELECT), KEY_SELECT},
        { generalGroup.readEntry("ButtonUp", (int) CEC_USER_CONTROL_CODE_UP), KEY_UP},
        { generalGroup.readEntry("ButtonDown", (int) CEC_USER_CONTROL_CODE_DOWN), KEY_DOWN},
        { generalGroup.readEntry("ButtonLeft", (int) CEC_USER_CONTROL_CODE_LEFT), KEY_LEFT},
        { generalGroup.readEntry("ButtonRight", (int) CEC_USER_CONTROL_CODE_RIGHT), KEY_RIGHT},
        { generalGroup.readEntry("ButtonNumber0", (int) CEC_USER_CONTROL_CODE_NUMBER0), KEY_0},
        { generalGroup.readEntry("ButtonNumber1", (int) CEC_USER_CONTROL_CODE_NUMBER1), KEY_1},
        { generalGroup.readEntry("ButtonNumber2", (int) CEC_USER_CONTROL_CODE_NUMBER2), KEY_2},
        { generalGroup.readEntry("ButtonNumber3", (int) CEC_USER_CONTROL_CODE_NUMBER3), KEY_3},
        { generalGroup.readEntry("ButtonNumber4", (int) CEC_USER_CONTROL_CODE_NUMBER4), KEY_4},
        { generalGroup.readEntry("ButtonNumber5", (int) CEC_USER_CONTROL_CODE_NUMBER5), KEY_5},
        { generalGroup.readEntry("ButtonNumber6", (int) CEC_USER_CONTROL_CODE_NUMBER6), KEY_6},
        { generalGroup.readEntry("ButtonNumber7", (int) CEC_USER_CONTROL_CODE_NUMBER7), KEY_7},
        { generalGroup.readEntry("ButtonNumber8", (int) CEC_USER_CONTROL_CODE_NUMBER8), KEY_8},
        { generalGroup.readEntry("ButtonNumber9", (int) CEC_USER_CONTROL_CODE_NUMBER9), KEY_9},
        { generalGroup.readEntry("ButtonBlue", (int) CEC_USER_CONTROL_CODE_F1_BLUE), KEY_BLUE},
        { generalGroup.readEntry("ButtonRed", (int) CEC_USER_CONTROL_CODE_F2_RED), KEY_RED},
        { generalGroup.readEntry("ButtonGreen", (int) CEC_USER_CONTROL_CODE_F3_GREEN), KEY_GREEN},
        { generalGroup.readEntry("ButtonYellow", (int) CEC_USER_CONTROL_CODE_F4_YELLOW), KEY_YELLOW},
        { generalGroup.readEntry("ButtonChannelUp", (int) CEC_USER_CONTROL_CODE_CHANNEL_UP), KEY_CHANNELUP},
        { generalGroup.readEntry("ButtonChannelDown", (int) CEC_USER_CONTROL_CODE_CHANNEL_DOWN), KEY_CHANNELDOWN},
        { generalGroup.readEntry("ButtonExit", (int) CEC_USER_CONTROL_CODE_EXIT), KEY_EXIT},
        { generalGroup.readEntry("ButtonBack", (int) CEC_USER_CONTROL_CODE_AN_RETURN), KEY_BACK},
        { generalGroup.readEntry("ButtonHome", (int) CEC_USER_CONTROL_CODE_ROOT_MENU), KEY_HOME},
        { generalGroup.readEntry("ButtonSubtitle", (int) CEC_USER_CONTROL_CODE_SUB_PICTURE), KEY_SUBTITLE},
        { generalGroup.readEntry("ButtonInfo", (int) CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION), KEY_INFO},
    };

    m_cecCallbacks.Clear();
    m_cecCallbacks.keyPress = &CECInput::handleCecKeypress;
    m_cecCallbacks.logMessage = &CECInput::handleCecLogMessage;

    libcec_configuration cecConfig;
    cecConfig.Clear();
    cecConfig.bActivateSource = 0;
    snprintf(cecConfig.strDeviceName, LIBCEC_OSD_NAME_SIZE, "joyclick");
    cecConfig.clientVersion = LIBCEC_VERSION_CURRENT;
    cecConfig.deviceTypes.Add(CEC_DEVICE_TYPE_RECORDING_DEVICE);

    ICECCallbacks cecCallbacks;
    cecConfig.callbacks = &m_cecCallbacks;

    m_cecAdapter = LibCecInitialise(&cecConfig);

    if (!m_cecAdapter) {
        qCritical() << "Could not create CEC adaptor with current config";
        deleteLater();
        return;
    }

    // Init video on targets that need this
    m_cecAdapter->InitVideoStandalone();
    m_opened = true;
}

void CECInput::run()
{
    // TODO: keep trying to detect a device till we find one
    cec_adapter_descriptor devices[1];
    int devices_found = 0;

    while (devices_found == 0) {
        devices_found = m_cecAdapter->DetectAdapters(devices, 1, NULL);
    }

    if (!m_cecAdapter->Open(devices[0].strComName)) {
        qWarning() << "Could not open CEC device " << devices[0].strComPath << " " << devices[0].strComName;
        deleteLater();
        return;
    }

    qDebug() << "Succesfully opened CEC device";

    // Just live forever so we can catch CEC events
    while (true) {}
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

/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "bashdetector.h"
#include "plugin.h"
#include "input_event_spy.h"

class QAudioOutput;
class QMediaPlayer;

namespace KWin
{

class DuHastPlayer : public Plugin, public InputEventSpy
{
    Q_OBJECT

public:
    DuHastPlayer();
    ~DuHastPlayer() override;

    void keyEvent(KeyEvent *event) override;

private:
    BashDetector m_bashDetector;
    std::unique_ptr<QAudioOutput> m_audioOutput;
    std::unique_ptr<QMediaPlayer> m_player;
};

} // namespace KWin

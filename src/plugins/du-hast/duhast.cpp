/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "duhast.h"
#include "duhastconfig.h"
#include "input.h"
#include "input_event.h"
#include "main.h"

#include <QAudioOutput>
#include <QDebug>
#include <QMediaPlayer>

namespace KWin
{

DuHastPlayer::DuHastPlayer()
    : m_audioOutput(std::make_unique<QAudioOutput>())
    , m_player(std::make_unique<QMediaPlayer>())
{
    DuHastConfig::instance(kwinApp()->config());
    m_player->setAudioOutput(m_audioOutput.get());

    const QString angrySong = DuHastConfig::angrySong();
    if (!angrySong.isEmpty()) {
        m_player->setSource(QUrl::fromLocalFile(DuHastConfig::angrySong()));
    }

    input()->installInputEventSpy(this);
}

DuHastPlayer::~DuHastPlayer()
{
}

void DuHastPlayer::keyEvent(KeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        m_player->stop();
    }

    if (m_bashDetector.update(event)) {
        qDebug() << "bash";
        if (m_player->isPlaying()) {
            m_audioOutput->setVolume(std::clamp(m_audioOutput->volume() + 0.3f, 0.0f, 1.0f));
        } else {
            m_audioOutput->setVolume(0.3f);
            m_player->play();
        }
    }
}

} // namespace KWin

#include "moc_duhast.cpp"

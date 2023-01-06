/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "cursorlocator.h"
#include "composite.h"
#include "input.h"
#include "input_event.h"
#include "scene/cursorscene.h"

#include <QDebug>

namespace KWin
{

CursorLocator::CursorLocator()
{
    input()->installInputEventSpy(this);
}

void CursorLocator::pointerEvent(MouseEvent *event)
{
    const std::chrono::microseconds timeDiff = event->timestamp() - m_lastTimestamp;
    m_lastTimestamp = event->timestamp();

    const qreal speed = event->delta().manhattanLength() / (timeDiff.count() * 0.000'001);
    // create a rolling average over 0.1s; estimating the speed from a single delta alone isn't accurate
    const auto speedRatio = std::min((timeDiff.count() * 0.000'001) / 0.1, 1.0);
    m_pointerSpeed = (1 - speedRatio) * m_pointerSpeed + speedRatio * speed;

    if (CursorScene *scene = Compositor::self()->cursorScene()) {
        scene->setScale(std::clamp((m_pointerSpeed - 1000.0) / 1000.0, 1.0, 3.0));
    }
}

} // namespace KWin

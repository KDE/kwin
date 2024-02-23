/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eventlistener.h"
#include "kwin/input.h"
#include "kwin/input_event.h"

#include <QDebug>

namespace KWin
{

EventListener::EventListener()
{
    qDebug() << "Loaded demo event listener plugin";
    input()->installInputEventSpy(this);
}

void EventListener::keyEvent(KeyEvent *event)
{
    qDebug() << event;
}

void EventListener::pointerEvent(MouseEvent *event)
{
    qDebug() << event;
}

} // namespace KWin

#include "moc_eventlistener.cpp"

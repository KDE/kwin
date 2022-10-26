/*
    SPDX-FileCopyrightText: 2016 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/
#include "osd.h"
#include "main.h"
#include "onscreennotification.h"
#include "scripting/scripting.h"
#include "workspace.h"

#include <QQmlEngine>
#include <QThread>

namespace KWin
{
namespace OSD
{

static OnScreenNotification *create()
{
    auto osd = new OnScreenNotification(workspace());
    osd->setConfig(kwinApp()->config());
    osd->setEngine(Scripting::self()->qmlEngine());
    return osd;
}

static OnScreenNotification *osd()
{
    static OnScreenNotification *s_osd = create();
    return s_osd;
}

void show(const QString &message, const QString &iconName, int timeout)
{
    if (!kwinApp()->shouldUseWaylandForCompositing()) {
        // FIXME: only supported on Wayland
        return;
    }

    if (QThread::currentThread() != qGuiApp->thread()) {
        QTimer::singleShot(0, QCoreApplication::instance(), [message, iconName, timeout] {
            show(message, iconName, timeout);
        });
        return;
    }

    auto notification = osd();
    notification->setIconName(iconName);
    notification->setMessage(message);
    notification->setTimeout(timeout);
    notification->setVisible(true);
}

void show(const QString &message, int timeout)
{
    show(message, QString(), timeout);
}

void show(const QString &message, const QString &iconName)
{
    show(message, iconName, 0);
}

void hide(HideFlags flags)
{
    if (!kwinApp()->shouldUseWaylandForCompositing()) {
        // FIXME: only supported on Wayland
        return;
    }
    osd()->setSkipCloseAnimation(flags.testFlag(HideFlag::SkipCloseAnimation));
    osd()->setVisible(false);
}

}
}

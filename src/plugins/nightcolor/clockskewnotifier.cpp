/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clockskewnotifier.h"
#include "clockskewnotifierengine_p.h"

namespace KWin
{

class ClockSkewNotifier::Private
{
public:
    void loadNotifierEngine();
    void unloadNotifierEngine();

    ClockSkewNotifier *notifier = nullptr;
    ClockSkewNotifierEngine *engine = nullptr;
    bool isActive = false;
};

void ClockSkewNotifier::Private::loadNotifierEngine()
{
    engine = ClockSkewNotifierEngine::create(notifier);

    if (engine) {
        QObject::connect(engine, &ClockSkewNotifierEngine::clockSkewed, notifier, &ClockSkewNotifier::clockSkewed);
    }
}

void ClockSkewNotifier::Private::unloadNotifierEngine()
{
    if (!engine) {
        return;
    }

    QObject::disconnect(engine, &ClockSkewNotifierEngine::clockSkewed, notifier, &ClockSkewNotifier::clockSkewed);
    engine->deleteLater();

    engine = nullptr;
}

ClockSkewNotifier::ClockSkewNotifier(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->notifier = this;
}

ClockSkewNotifier::~ClockSkewNotifier()
{
}

bool ClockSkewNotifier::isActive() const
{
    return d->isActive;
}

void ClockSkewNotifier::setActive(bool set)
{
    if (d->isActive == set) {
        return;
    }

    d->isActive = set;

    if (d->isActive) {
        d->loadNotifierEngine();
    } else {
        d->unloadNotifierEngine();
    }

    Q_EMIT activeChanged();
}

} // namespace KWin

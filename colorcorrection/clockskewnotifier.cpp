/*
 * Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

    emit activeChanged();
}

} // namespace KWin

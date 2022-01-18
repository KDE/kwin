/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens.h"
#include <abstract_client.h>
#include "abstract_output.h"
#include "cursor.h"
#include "utils/common.h"
#include "settings.h"
#include <workspace.h>
#include <config-kwin.h>
#include "platform.h"

namespace KWin
{

Screens *Screens::s_self = nullptr;
Screens *Screens::create(QObject *parent)
{
    Q_ASSERT(!s_self);
    s_self = new Screens(parent);
    Q_ASSERT(s_self);
    s_self->init();
    return s_self;
}

Screens::Screens(QObject *parent)
    : QObject(parent)
    , m_count(0)
    , m_maxScale(1.0)
{
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::updateCount);
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::changed);
}

Screens::~Screens()
{
    s_self = nullptr;
}

void Screens::init()
{
    updateCount();
    connect(this, &Screens::countChanged, this, &Screens::changed, Qt::QueuedConnection);
    connect(this, &Screens::changed, this, &Screens::updateSize);
    connect(this, &Screens::sizeChanged, this, &Screens::geometryChanged);

    Q_EMIT changed();
}

QRect Screens::geometry(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->geometry();
    }
    return QRect();
}

qreal Screens::scale(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->scale();
    }
    return 1.0;
}

qreal Screens::maxScale() const
{
    return m_maxScale;
}

void Screens::updateSize()
{
    QRect bounding;
    qreal maxScale = 1.0;
    for (int i = 0; i < count(); ++i) {
        bounding = bounding.united(geometry(i));
        maxScale = qMax(maxScale, scale(i));
    }
    if (m_boundingSize != bounding.size()) {
        m_boundingSize = bounding.size();
        Q_EMIT sizeChanged();
    }
    if (!qFuzzyCompare(m_maxScale, maxScale)) {
        m_maxScale = maxScale;
        Q_EMIT maxScaleChanged();
    }
}

void Screens::updateCount()
{
    setCount(kwinApp()->platform()->enabledOutputs().size());
}

void Screens::setCount(int count)
{
    if (m_count == count) {
        return;
    }
    const int previous = m_count;
    m_count = count;
    Q_EMIT countChanged(previous, count);
}

int Screens::intersecting(const QRect &r) const
{
    int cnt = 0;
    for (int i = 0; i < count(); ++i) {
        if (geometry(i).intersects(r)) {
            ++cnt;
        }
    }
    return cnt;
}

int Screens::number(const QPoint &pos) const
{
    return kwinApp()->platform()->enabledOutputs().indexOf(kwinApp()->platform()->outputAt(pos));
}

AbstractOutput *Screens::findOutput(int screen) const
{
    return kwinApp()->platform()->findOutput(screen);
}

} // namespace

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens.h"

#include <config-kwin.h>

#include "cursor.h"
#include "output.h"
#include "platform.h"
#include "settings.h"
#include "utils/common.h"
#include <window.h>
#include <workspace.h>

namespace KWin
{

Screens::Screens()
    : m_count(0)
    , m_maxScale(1.0)
{
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::updateCount);
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::changed);
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
    if (Output *output = findOutput(screen)) {
        return output->geometry();
    }
    return QRect();
}

qreal Screens::scale(int screen) const
{
    if (Output *output = findOutput(screen)) {
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
    setCount(workspace()->outputs().size());
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

Output *Screens::findOutput(int screen) const
{
    return workspace()->outputs().value(screen);
}

int Screens::count() const
{
    return m_count;
}

QSize Screens::size() const
{
    return m_boundingSize;
}

QRect Screens::geometry() const
{
    return QRect(QPoint(0, 0), size());
}

} // namespace

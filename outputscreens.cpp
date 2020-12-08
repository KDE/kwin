/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "outputscreens.h"
#include "platform.h"
#include "abstract_output.h"

namespace KWin
{

OutputScreens::OutputScreens(Platform *platform, QObject *parent)
    : Screens(parent),
      m_platform(platform)
{
}

OutputScreens::~OutputScreens() = default;

void OutputScreens::init()
{
    updateCount();
    KWin::Screens::init();
    emit changed();
}

QString OutputScreens::name(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->name();
    }
    return QString();
}

bool OutputScreens::isInternal(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->isInternal();
    }
    return false;
}

QRect OutputScreens::geometry(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->geometry();
    }
    return QRect();
}

QSize OutputScreens::size(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->geometry().size();
    }
    return QSize();
}

qreal OutputScreens::scale(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->scale();
    }
    return 1.0;
}

QSizeF OutputScreens::physicalSize(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->physicalSize();
    }
    return QSizeF();
}

float OutputScreens::refreshRate(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->refreshRate() / 1000.0;
    }
    return 60.0;
}

void OutputScreens::updateCount()
{
    setCount(m_platform->enabledOutputs().size());
}

int OutputScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    const auto outputs = m_platform->enabledOutputs();
    for (int i = 0; i < outputs.size(); ++i) {
        const QRect &geo = outputs[i]->geometry();
        if (geo.contains(pos)) {
            return i;
        }
        int distance = QPoint(geo.topLeft() - pos).manhattanLength();
        distance = qMin(distance, QPoint(geo.topRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geo.bottomRight() - pos).manhattanLength());
        distance = qMin(distance, QPoint(geo.bottomLeft() - pos).manhattanLength());
        if (distance < minDistance) {
            minDistance = distance;
            bestScreen = i;
        }
    }
    return bestScreen;
}

AbstractOutput *OutputScreens::findOutput(int screen) const
{
    return m_platform->findOutput(screen);
}

} // namespace

/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "screens_drm.h"
#include "drm_backend.h"
#include "drm_output.h"

namespace KWin
{

DrmScreens::DrmScreens(DrmBackend *backend, QObject *parent)
    : Screens(parent)
    , m_backend(backend)
{
    connect(backend, &DrmBackend::screensQueried, this, &DrmScreens::updateCount);
    connect(backend, &DrmBackend::screensQueried, this, &DrmScreens::changed);
}

DrmScreens::~DrmScreens() = default;

void DrmScreens::init()
{
    updateCount();
    KWin::Screens::init();
    emit changed();
}

QRect DrmScreens::geometry(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return QRect();
    }
    return outputs.at(screen)->geometry();
}

qreal DrmScreens::scale(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return 1;
    }
    return outputs.at(screen)->scale();
}

QSize DrmScreens::size(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return QSize();
    }
    return outputs.at(screen)->geometry().size();
}

void DrmScreens::updateCount()
{
    setCount(m_backend->enabledOutputs().size());
}

int DrmScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    const auto outputs = m_backend->enabledOutputs();
    for (int i = 0; i < outputs.size(); ++i) {
        const QRect &geo = outputs.at(i)->geometry();
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

QString DrmScreens::name(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return Screens::name(screen);
    }
    return outputs.at(screen)->name();
}

float DrmScreens::refreshRate(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return Screens::refreshRate(screen);
    }
    return outputs.at(screen)->currentRefreshRate() / 1000.0f;
}

QSizeF DrmScreens::physicalSize(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return Screens::physicalSize(screen);
    }
    return outputs.at(screen)->physicalSize();
}

bool DrmScreens::isInternal(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return false;
    }
    return outputs.at(screen)->isInternal();
}

bool DrmScreens::supportsTransformations(int screen) const
{
    const auto outputs = m_backend->enabledOutputs();
    if (screen >= outputs.size()) {
        return false;
    }
    return outputs.at(screen)->supportsTransformations();
}

Qt::ScreenOrientation DrmScreens::orientation(int screen) const
{
    const auto outputs = m_backend->outputs();
    if (screen >= outputs.size()) {
        return Qt::PrimaryOrientation;
    }
    return outputs.at(screen)->orientation();
}

}

/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screens.h"
#include <abstract_client.h>
#include <client.h>
#include "cursor.h"
#include "orientation_sensor.h"
#include "utils.h"
#include "settings.h"
#include <workspace.h>
#include <config-kwin.h>
#include "platform.h"
#include "wayland_server.h"
#ifdef KWIN_UNIT_TEST
#include <mock_screens.h>
#endif

namespace KWin
{

Screens *Screens::s_self = nullptr;
Screens *Screens::create(QObject *parent)
{
    Q_ASSERT(!s_self);
#ifdef KWIN_UNIT_TEST
    s_self = new MockScreens(parent);
#else
    s_self = kwinApp()->platform()->createScreens(parent);
#endif
    Q_ASSERT(s_self);
    s_self->init();
    return s_self;
}

Screens::Screens(QObject *parent)
    : QObject(parent)
    , m_count(0)
    , m_current(0)
    , m_currentFollowsMouse(false)
    , m_changedTimer(new QTimer(this))
    , m_orientationSensor(new OrientationSensor(this))
{
    connect(this, &Screens::changed, this,
        [this] {
            int internalIndex = -1;
            for (int i = 0; i < m_count; i++) {
                if (isInternal(i)) {
                    internalIndex = i;
                    break;
                }
            }
            m_orientationSensor->setEnabled(internalIndex != -1 && supportsTransformations(internalIndex));
        }
    );
}

Screens::~Screens()
{
    s_self = NULL;
}

void Screens::init()
{
    m_changedTimer->setSingleShot(true);
    m_changedTimer->setInterval(100);
    connect(m_changedTimer, SIGNAL(timeout()), SLOT(updateCount()));
    connect(m_changedTimer, SIGNAL(timeout()), SIGNAL(changed()));
    connect(this, &Screens::countChanged, this, &Screens::changed, Qt::QueuedConnection);
    connect(this, &Screens::changed, this, &Screens::updateSize);
    connect(this, &Screens::sizeChanged, this, &Screens::geometryChanged);

    Settings settings;
    settings.setDefaults();
    m_currentFollowsMouse = settings.activeMouseScreen();
}

QString Screens::name(int screen) const
{
    Q_UNUSED(screen)
    qCWarning(KWIN_CORE, "%s::name(int screen) is a stub, please reimplement it!", metaObject()->className());
    return QLatin1String("DUMMY");
}

float Screens::refreshRate(int screen) const
{
    Q_UNUSED(screen)
    qCWarning(KWIN_CORE, "%s::refreshRate(int screen) is a stub, please reimplement it!", metaObject()->className());
    return 60.0f;
}

qreal Screens::scale(int screen) const
{
    Q_UNUSED(screen)
    return 1;
}

void Screens::reconfigure()
{
    if (!m_config) {
        return;
    }
    Settings settings(m_config);
    settings.read();
    setCurrentFollowsMouse(settings.activeMouseScreen());
}

void Screens::updateSize()
{
    QRect bounding;
    for (int i = 0; i < count(); ++i) {
        bounding = bounding.united(geometry(i));
    }
    if (m_boundingSize != bounding.size()) {
        m_boundingSize = bounding.size();
        emit sizeChanged();
    }
}

void Screens::setCount(int count)
{
    if (m_count == count) {
        return;
    }
    const int previous = m_count;
    m_count = count;
    emit countChanged(previous, count);
}

void Screens::setCurrent(int current)
{
    if (m_current == current) {
        return;
    }
    m_current = current;
    emit currentChanged();
}

void Screens::setCurrent(const QPoint &pos)
{
    setCurrent(number(pos));
}

void Screens::setCurrent(const AbstractClient *c)
{
    if (!c->isActive()) {
        return;
    }
    if (!c->isOnScreen(m_current)) {
        setCurrent(c->screen());
    }
}

void Screens::setCurrentFollowsMouse(bool follows)
{
    if (m_currentFollowsMouse == follows) {
        return;
    }
    m_currentFollowsMouse = follows;
}

int Screens::current() const
{
    if (m_currentFollowsMouse) {
        return number(Cursor::pos());
    }
    AbstractClient *client = Workspace::self()->activeClient();
    if (client && !client->isOnScreen(m_current)) {
        return client->screen();
    }
    return m_current;
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

QSize Screens::displaySize() const
{
    return size();
}

QSizeF Screens::physicalSize(int screen) const
{
    return QSizeF(size(screen)) / 3.8;
}

bool Screens::isInternal(int screen) const
{
    Q_UNUSED(screen)
    return false;
}

bool Screens::supportsTransformations(int screen) const
{
    Q_UNUSED(screen)
    return false;
}

Qt::ScreenOrientation Screens::orientation(int screen) const
{
    Q_UNUSED(screen)
    return Qt::PrimaryOrientation;
}

void Screens::setConfig(KSharedConfig::Ptr config)
{
    m_config = config;
    if (m_orientationSensor) {
        m_orientationSensor->setConfig(config);
    }
}

BasicScreens::BasicScreens(Platform *backend, QObject *parent)
    : Screens(parent)
    , m_backend(backend)
{
}

BasicScreens::~BasicScreens() = default;

void BasicScreens::init()
{
    updateCount();
    KWin::Screens::init();
#ifndef KWIN_UNIT_TEST
    connect(m_backend, &Platform::screenSizeChanged,
            this, &BasicScreens::startChangedTimer);
#endif
    emit changed();
}

QRect BasicScreens::geometry(int screen) const
{
    if (screen < m_geometries.count()) {
        return m_geometries.at(screen);
    }
    return QRect();
}

QSize BasicScreens::size(int screen) const
{
    if (screen < m_geometries.count()) {
        return m_geometries.at(screen).size();
    }
    return QSize();
}

qreal BasicScreens::scale(int screen) const
{
    if (screen < m_scales.count()) {
        return m_scales.at(screen);
    }
    return 1;
}

void BasicScreens::updateCount()
{
    m_geometries = m_backend->screenGeometries();
    m_scales = m_backend->screenScales();
    setCount(m_geometries.count());
}

int BasicScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    for (int i = 0; i < m_geometries.count(); ++i) {
        const QRect &geo = m_geometries.at(i);
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

} // namespace

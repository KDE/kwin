/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens.h"
#include <abstract_client.h>
#include <x11client.h>
#include "cursor.h"
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
    , m_maxScale(1.0)
{
}

Screens::~Screens()
{
    s_self = nullptr;
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

qreal Screens::maxScale() const
{
    return m_maxScale;
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
    qreal maxScale = 1.0;
    for (int i = 0; i < count(); ++i) {
        bounding = bounding.united(geometry(i));
        maxScale = qMax(maxScale, scale(i));
    }
    if (m_boundingSize != bounding.size()) {
        m_boundingSize = bounding.size();
        emit sizeChanged();
    }
    if (!qFuzzyCompare(m_maxScale, maxScale)) {
        m_maxScale = maxScale;
        emit maxScaleChanged();
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
        return number(Cursors::self()->mouse()->pos());
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
}

int Screens::physicalDpiX(int screen) const
{
    return size(screen).width() / physicalSize(screen).width() * qreal(25.4);
}

int Screens::physicalDpiY(int screen) const
{
    return size(screen).height() / physicalSize(screen).height() * qreal(25.4);
}

} // namespace

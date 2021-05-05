/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens.h"
#include <abstract_client.h>
#include "abstract_output.h"
#include <x11client.h>
#include "cursor.h"
#include "utils.h"
#include "settings.h"
#include <workspace.h>
#include <config-kwin.h>
#include "platform.h"
#include "wayland_server.h"
#include "abstract_wayland_output.h"
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
    s_self = new Screens(parent);
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
    , m_maxScale(1.0)
{
    // TODO: Do something about testScreens and other tests that use MockScreens.
    // They only make core code more convoluted with ifdefs.
#ifndef KWIN_UNIT_TEST
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::updateCount);
    connect(kwinApp()->platform(), &Platform::screensQueried, this, &Screens::changed);
#endif
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

    Settings settings;
    settings.setDefaults();
    m_currentFollowsMouse = settings.activeMouseScreen();
    emit changed();
}

QString Screens::name(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->name();
    }
    return QString();
}

bool Screens::isInternal(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->isInternal();
    }
    return false;
}

QRect Screens::geometry(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->geometry();
    }
    return QRect();
}

QSize Screens::size(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->geometry().size();
    }
    return QSize();
}

qreal Screens::scale(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->scale();
    }
    return 1.0;
}

QSizeF Screens::physicalSize(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->physicalSize();
    }
    return QSizeF();
}

float Screens::refreshRate(int screen) const
{
    if (AbstractOutput *output = findOutput(screen)) {
        return output->refreshRate() / 1000.0;
    }
    return 60.0;
}

qreal Screens::maxScale() const
{
    return m_maxScale;
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

bool Screens::isVrrCapable(int screen) const
{
#ifdef KWIN_UNIT_TEST
    Q_UNUSED(screen);
    return false;
#else
    if (auto output = findOutput(screen)) {
        if (auto waylandoutput = dynamic_cast<AbstractWaylandOutput*>(output)) {
            return waylandoutput->capabilities() & AbstractWaylandOutput::Capability::Vrr;
        }
    }
#endif
    return true;
}

RenderLoop::VrrPolicy Screens::vrrPolicy(int screen) const
{
#ifdef KWIN_UNIT_TEST
    Q_UNUSED(screen);
    return RenderLoop::VrrPolicy::Never;
#else
    if (auto output = findOutput(screen)) {
        if (auto waylandOutput = dynamic_cast<AbstractWaylandOutput *>(output)) {
            return waylandOutput->vrrPolicy();
        }
    }
    return RenderLoop::VrrPolicy::Never;
#endif
}

int Screens::number(const QPoint &pos) const
{
    // TODO: Do something about testScreens and other tests that use MockScreens.
    // They only make core code more convoluted with ifdefs.
#ifdef KWIN_UNIT_TEST
    Q_UNUSED(pos)
    return -1;
#else
    int bestScreen = 0;
    int minDistance = INT_MAX;
    const auto outputs = kwinApp()->platform()->enabledOutputs();
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
#endif
}

AbstractOutput *Screens::findOutput(int screen) const
{
    // TODO: Do something about testScreens and other tests that use MockScreens.
    // They only make core code more convoluted with ifdefs.
#ifdef KWIN_UNIT_TEST
    Q_UNUSED(screen)
    return nullptr;
#else
    return kwinApp()->platform()->findOutput(screen);
#endif
}

} // namespace

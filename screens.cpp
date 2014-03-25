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
#include "client.h"
#include "cursor.h"
#include "settings.h"
#include "workspace.h"
#if HAVE_WAYLAND
#include "wayland_backend.h"
#include <xcb/randr.h>
#endif

#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>

namespace KWin
{

Screens *Screens::s_self = nullptr;
Screens *Screens::create(QObject *parent)
{
    Q_ASSERT(!s_self);
#if HAVE_WAYLAND
    if (kwinApp()->operationMode() == Application::OperationModeWaylandAndX11) {
        s_self = new WaylandScreens(parent);
        return s_self;
    }
#endif
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        s_self = new DesktopWidgetScreens(parent);
    }
    s_self->init();
    return s_self;
}

Screens::Screens(QObject *parent)
    : QObject(parent)
    , m_count(0)
    , m_current(0)
    , m_currentFollowsMouse(false)
    , m_changedTimer(new QTimer(this))
{
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
    connect(this, &Screens::countChanged, this, &Screens::changed);
    connect(this, &Screens::changed, this, &Screens::updateSize);
    connect(this, &Screens::sizeChanged, this, &Screens::geometryChanged);

    Settings settings;
    settings.setDefaults();
    m_currentFollowsMouse = settings.activeMouseScreen();
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

void Screens::setCurrent(const Client *c)
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
    Client *client = Workspace::self()->activeClient();
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

DesktopWidgetScreens::DesktopWidgetScreens(QObject *parent)
    : Screens(parent)
    , m_desktop(QApplication::desktop())
{
}

DesktopWidgetScreens::~DesktopWidgetScreens()
{
}

void DesktopWidgetScreens::init()
{
    Screens::init();
    connect(m_desktop, SIGNAL(screenCountChanged(int)), SLOT(startChangedTimer()));
    connect(m_desktop, SIGNAL(resized(int)), SLOT(startChangedTimer()));
    updateCount();
}

QRect DesktopWidgetScreens::geometry(int screen) const
{
    if (Screens::self()->isChanging())
        const_cast<DesktopWidgetScreens*>(this)->updateCount();
    return m_desktop->screenGeometry(screen);
}

QSize DesktopWidgetScreens::size(int screen) const
{
    return geometry(screen).size();
}

int DesktopWidgetScreens::number(const QPoint &pos) const
{
    if (Screens::self()->isChanging())
        const_cast<DesktopWidgetScreens*>(this)->updateCount();
    return m_desktop->screenNumber(pos);
}

void DesktopWidgetScreens::updateCount()
{
    setCount(m_desktop->screenCount());
}

#if HAVE_WAYLAND
WaylandScreens::WaylandScreens(QObject* parent)
    : Screens(parent)
{
}

WaylandScreens::~WaylandScreens()
{
}

void WaylandScreens::init()
{
    Screens::init();
    connect(Wayland::WaylandBackend::self(), &Wayland::WaylandBackend::outputsChanged,
            this, &WaylandScreens::startChangedTimer);
    updateCount();
}

QRect WaylandScreens::geometry(int screen) const
{
    if (screen >= m_geometries.size()) {
        return QRect();
    }
    return m_geometries.at(screen);
}

QSize WaylandScreens::size(int screen) const
{
    return geometry(screen).size();
}

int WaylandScreens::number(const QPoint &pos) const
{
    int bestScreen = 0;
    int minDistance = INT_MAX;
    for (int i = 0; i < m_geometries.size(); ++i) {
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

void WaylandScreens::updateCount()
{
    m_geometries.clear();
    int count = 0;
    const QList<Wayland::Output*> &outputs = Wayland::WaylandBackend::self()->outputs();
    for (auto it = outputs.begin(); it != outputs.end(); ++it) {
        if ((*it)->pixelSize().isEmpty()) {
            continue;
        }
        count++;
        m_geometries.append(QRect((*it)->globalPosition(), (*it)->pixelSize()));
    }
    if (m_geometries.isEmpty()) {
        // we need a fake screen
        m_geometries.append(QRect(0, 0, displayWidth(), displayHeight()));
        setCount(1);
        return;
    }
    updateXRandr();
    emit changed();
}

namespace RandR
{
typedef Xcb::Wrapper<xcb_randr_get_screen_resources_current_reply_t,
                     xcb_randr_get_screen_resources_current_cookie_t,
                     &xcb_randr_get_screen_resources_current_reply,
                     &xcb_randr_get_screen_resources_current_unchecked> CurrentResources;
}

static bool setNewScreenSize(const QSize &size)
{
    auto c = xcb_randr_set_screen_size_checked(connection(), rootWindow(), size.width(), size.height(), 1, 1);
    ScopedCPointer<xcb_generic_error_t> error(xcb_request_check(connection(), c));
    if (!error.isNull()) {
        qDebug() << "Error setting screen size: " << error->error_code;
        return false;
    }
    return true;
}

static xcb_randr_crtc_t getCrtc(const xcb_randr_get_screen_resources_current_reply_t* r)
{
    if (xcb_randr_get_screen_resources_current_crtcs_length(r) == 0) {
        qDebug() << "No CRTCs";
        return XCB_NONE;
    }
    return xcb_randr_get_screen_resources_current_crtcs(r)[0];
}

static xcb_randr_output_t getOutputForCrtc(xcb_randr_crtc_t crtc)
{
    ScopedCPointer<xcb_randr_get_crtc_info_reply_t> info(xcb_randr_get_crtc_info_reply(
        connection(), xcb_randr_get_crtc_info(connection(), crtc, XCB_TIME_CURRENT_TIME), nullptr));
    if (info->num_outputs == 0) {
        return XCB_NONE;
    }
    return xcb_randr_get_crtc_info_outputs(info.data())[0];
}

static xcb_randr_mode_t createNewMode(const QSize &size)
{
    // need to create the new mode
    qDebug() << "Creating a new mode";
    QString name(QString::number(size.width()));
    name.append('x');
    name.append(QString::number(size.height()));
    xcb_randr_mode_info_t newInfo;
    newInfo.dot_clock = 0;
    newInfo.height = size.height();
    newInfo.hskew = 0;
    newInfo.hsync_end = 0;
    newInfo.hsync_start = 0;
    newInfo.htotal = size.width();
    newInfo.id = 0;
    newInfo.mode_flags = 0;
    newInfo.vsync_end = 0;
    newInfo.vsync_start = 0;
    newInfo.vtotal = size.height();
    newInfo.width = size.width();
    newInfo.name_len = name.length();
    auto cookie = xcb_randr_create_mode_unchecked(connection(), rootWindow(), newInfo, name.length(), name.toUtf8().constData());
    ScopedCPointer<xcb_randr_create_mode_reply_t> reply(xcb_randr_create_mode_reply(connection(), cookie, nullptr));
    if (!reply.isNull()) {
        return reply->mode;
    }
    return XCB_NONE;
}

static xcb_randr_mode_t getModeForSize(const QSize &size, const xcb_randr_get_screen_resources_current_reply_t* r)
{
    xcb_randr_mode_info_t *infos = xcb_randr_get_screen_resources_current_modes(r);
    const int modeInfoLength = xcb_randr_get_screen_resources_current_modes_length(r);
    // check available modes
    for (int i = 0; i < modeInfoLength; ++i) {
        xcb_randr_mode_info_t modeInfo = infos[i];
        if (modeInfo.width == size.width() && modeInfo.height == size.height()) {
            qDebug() << "Found our required mode";
            return modeInfo.id;
        }
    }
    // did not find our mode, so create it
    return createNewMode(size);
}

static bool addModeToOutput(xcb_randr_output_t output, xcb_randr_mode_t mode)
{
    ScopedCPointer<xcb_randr_get_output_info_reply_t> info(xcb_randr_get_output_info_reply(connection(),
        xcb_randr_get_output_info(connection(), output, XCB_TIME_CURRENT_TIME), nullptr));
    xcb_randr_mode_t *modes = xcb_randr_get_output_info_modes(info.data());
    for (int i = 0; i < info->num_modes; ++i) {
        if (modes[i] == mode) {
            return true;
        }
    }
    qDebug() << "Need to add the mode to output";
    auto c = xcb_randr_add_output_mode_checked(connection(), output, mode);
    ScopedCPointer<xcb_generic_error_t> error(xcb_request_check(connection(), c));
    if (!error.isNull()) {
        qDebug() << "Error while adding mode to output: " << error->error_code;
        return false;
    }
    return true;
}

void WaylandScreens::updateXRandr()
{
    if (!Xcb::Extensions::self()->isRandrAvailable()) {
        qDebug() << "No RandR extension available, cannot sync with X";
        return;
    }
    QRegion screens;
    foreach (const QRect &rect, m_geometries) {
        screens = screens.united(rect);
    }
    const QSize &size = screens.boundingRect().size();
    if (size.isEmpty()) {
        return;
    }

    RandR::CurrentResources currentResources(rootWindow());
    xcb_randr_crtc_t crtc = getCrtc(currentResources.data());
    if (crtc == XCB_NONE) {
        return;
    }
    xcb_randr_output_t output = getOutputForCrtc(crtc);
    if (output == XCB_NONE) {
        return;
    }
    // first disable the first CRTC
    xcb_randr_set_crtc_config(connection(), crtc, XCB_TIME_CURRENT_TIME, XCB_TIME_CURRENT_TIME,
                              0, 0, XCB_NONE, XCB_RANDR_ROTATION_ROTATE_0, 0, nullptr);

    // then set new screen size
    if (!setNewScreenSize(size)) {
        return;
    }

    xcb_randr_mode_t mode = getModeForSize(size, currentResources.data());
    if (mode == XCB_NONE) {
        return;
    }

    if (!addModeToOutput(output, mode)) {
        return;
    }
    // enable CRTC again
    xcb_randr_set_crtc_config(connection(), crtc, XCB_TIME_CURRENT_TIME, XCB_TIME_CURRENT_TIME,
                              0, 0, mode, XCB_RANDR_ROTATION_ROTATE_0, 1, &output);
}

#endif

} // namespace

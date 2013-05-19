/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#include "xcbutils.h"
#include "utils.h"
// KDE
#include <KDE/KDebug>
// xcb
#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/randr.h>
#include <xcb/render.h>
#include <xcb/shape.h>
#include <xcb/sync.h>
#include <xcb/xfixes.h>

namespace KWin {

namespace Xcb {

static const int COMPOSITE_MAX_MAJOR = 0;
static const int COMPOSITE_MAX_MINOR = 4;
static const int DAMAGE_MAX_MAJOR = 1;
static const int DAMAGE_MIN_MAJOR = 1;
static const int SYNC_MAX_MAJOR = 3;
static const int SYNC_MAX_MINOR = 0;
static const int RANDR_MAX_MAJOR = 1;
static const int RANDR_MAX_MINOR = 4;
static const int RENDER_MAX_MAJOR = 0;
static const int RENDER_MAX_MINOR = 11;
static const int XFIXES_MAX_MAJOR = 5;
static const int XFIXES_MAX_MINOR = 0;

ExtensionData::ExtensionData()
    : version(0)
    , eventBase(0)
    , errorBase(0)
    , majorOpcode(0)
    , present(0)
{
}

template<typename reply, typename T, typename F>
void Extensions::initVersion(T cookie, F f, ExtensionData *dataToFill)
{
    ScopedCPointer<reply> version(f(connection(), cookie, NULL));
    dataToFill->version = version->major_version * 0x10 + version->minor_version;
}

Extensions *Extensions::s_self = NULL;

Extensions *Extensions::self()
{
    if (!s_self) {
        s_self = new Extensions();
    }
    return s_self;
}

void Extensions::destroy()
{
    delete s_self;
    s_self = NULL;
}

Extensions::Extensions()
{
    init();
}

Extensions::~Extensions()
{
}

void Extensions::init()
{
    xcb_connection_t *c = connection();
    xcb_prefetch_extension_data(c, &xcb_shape_id);
    xcb_prefetch_extension_data(c, &xcb_randr_id);
    xcb_prefetch_extension_data(c, &xcb_damage_id);
    xcb_prefetch_extension_data(c, &xcb_composite_id);
    xcb_prefetch_extension_data(c, &xcb_xfixes_id);
    xcb_prefetch_extension_data(c, &xcb_render_id);
    xcb_prefetch_extension_data(c, &xcb_sync_id);

    m_shape.name     = QByteArray("SHAPE");
    m_randr.name     = QByteArray("RANDR");
    m_damage.name    = QByteArray("DAMAGE");
    m_composite.name = QByteArray("Composite");
    m_fixes.name     = QByteArray("XFIXES");
    m_render.name    = QByteArray("RENDER");
    m_sync.name      = QByteArray("SYNC");

    extensionQueryReply(xcb_get_extension_data(c, &xcb_shape_id), &m_shape);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_randr_id), &m_randr);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_damage_id), &m_damage);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_composite_id), &m_composite);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_xfixes_id), &m_fixes);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_render_id), &m_render);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_sync_id), &m_sync);

    // extension specific queries
    xcb_shape_query_version_cookie_t shapeVersion;
    xcb_randr_query_version_cookie_t randrVersion;
    xcb_damage_query_version_cookie_t damageVersion;
    xcb_composite_query_version_cookie_t compositeVersion;
    xcb_xfixes_query_version_cookie_t xfixesVersion;
    xcb_render_query_version_cookie_t renderVersion;
    xcb_sync_initialize_cookie_t syncVersion;
    if (m_shape.present) {
        shapeVersion = xcb_shape_query_version_unchecked(c);
    }
    if (m_randr.present) {
        randrVersion = xcb_randr_query_version_unchecked(c, RANDR_MAX_MAJOR, RANDR_MAX_MINOR);
    }
    if (m_damage.present) {
        damageVersion = xcb_damage_query_version_unchecked(c, DAMAGE_MAX_MAJOR, DAMAGE_MIN_MAJOR);
    }
    if (m_composite.present) {
        compositeVersion = xcb_composite_query_version_unchecked(c, COMPOSITE_MAX_MAJOR, COMPOSITE_MAX_MINOR);
    }
    if (m_fixes.present) {
        xfixesVersion = xcb_xfixes_query_version_unchecked(c, XFIXES_MAX_MAJOR, XFIXES_MAX_MINOR);
    }
    if (m_render.present) {
        renderVersion = xcb_render_query_version_unchecked(c, RENDER_MAX_MAJOR, RENDER_MAX_MINOR);
    }
    if (m_sync.present) {
        syncVersion = xcb_sync_initialize(c, SYNC_MAX_MAJOR, SYNC_MAX_MINOR);
    }
    // handle replies
    if (m_shape.present) {
        initVersion<xcb_shape_query_version_reply_t>(shapeVersion, &xcb_shape_query_version_reply, &m_shape);
    }
    if (m_randr.present) {
        initVersion<xcb_randr_query_version_reply_t>(randrVersion, &xcb_randr_query_version_reply, &m_randr);
    }
    if (m_damage.present) {
        initVersion<xcb_damage_query_version_reply_t>(damageVersion, &xcb_damage_query_version_reply, &m_damage);
    }
    if (m_composite.present) {
        initVersion<xcb_composite_query_version_reply_t>(compositeVersion, &xcb_composite_query_version_reply, &m_composite);
    }
    if (m_fixes.present) {
        initVersion<xcb_xfixes_query_version_reply_t>(xfixesVersion, &xcb_xfixes_query_version_reply, &m_fixes);
    }
    if (m_render.present) {
        initVersion<xcb_render_query_version_reply_t>(renderVersion, &xcb_render_query_version_reply, &m_render);
    }
    if (m_sync.present) {
        initVersion<xcb_sync_initialize_reply_t>(syncVersion, &xcb_sync_initialize_reply, &m_sync);
    }
    kDebug(1212) << "Extensions: shape: 0x" << QString::number(m_shape.version, 16)
                 << " composite: 0x" << QString::number(m_composite.version, 16)
                 << " render: 0x" << QString::number(m_render.version, 16)
                 << " fixes: 0x" << QString::number(m_fixes.version, 16)
                 << " randr: 0x" << QString::number(m_randr.version, 16)
                 << " sync: 0x" << QString::number(m_sync.version, 16)
                 << " damage: 0x " << QString::number(m_damage.version, 16) << endl;
}

void Extensions::extensionQueryReply(const xcb_query_extension_reply_t *extension, ExtensionData *dataToFill)
{
    if (!extension) {
        return;
    }
    dataToFill->present = extension->present;
    dataToFill->eventBase = extension->first_event;
    dataToFill->errorBase = extension->first_error;
    dataToFill->majorOpcode = extension->major_opcode;
}

int Extensions::damageNotifyEvent() const
{
    return m_damage.eventBase + XCB_DAMAGE_NOTIFY;
}

bool Extensions::hasShape(xcb_window_t w) const
{
    if (!isShapeAvailable()) {
        return false;
    }
    ScopedCPointer<xcb_shape_query_extents_reply_t> extents(xcb_shape_query_extents_reply(
        connection(), xcb_shape_query_extents_unchecked(connection(), w), NULL));
    if (extents.isNull()) {
        return false;
    }
    return extents->bounding_shaped > 0;
}

bool Extensions::isCompositeOverlayAvailable() const
{
    return m_composite.version >= 0x03; // 0.3
}

bool Extensions::isFixesRegionAvailable() const
{
    return m_fixes.version >= 0x30; // 3
}

int Extensions::fixesCursorNotifyEvent() const
{
    return m_fixes.eventBase + XCB_XFIXES_CURSOR_NOTIFY;
}

bool Extensions::isShapeInputAvailable() const
{
    return m_shape.version >= 0x11; // 1.1
}

int Extensions::randrNotifyEvent() const
{
    return m_randr.eventBase + XCB_RANDR_SCREEN_CHANGE_NOTIFY;
}

int Extensions::shapeNotifyEvent() const
{
    return m_shape.eventBase + XCB_SHAPE_NOTIFY;
}

int Extensions::syncAlarmNotifyEvent() const
{
    return m_sync.eventBase + XCB_SYNC_ALARM_NOTIFY;
}

QVector<ExtensionData> Extensions::extensions() const
{
    QVector<ExtensionData> extensions;
    extensions << m_shape
               << m_randr
               << m_damage
               << m_composite
               << m_render
               << m_fixes
               << m_sync;
    return extensions;
}

} // namespace Xcb
} // namespace KWin

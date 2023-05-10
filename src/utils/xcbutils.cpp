/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "utils/xcbutils.h"
#include "utils/common.h"
#include <core/output.h>
#include <workspace.h>

// Qt
#include <QDebug>
// xcb
#include <cmath>
#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/glx.h>
#include <xcb/randr.h>
#include <xcb/render.h>
#include <xcb/shape.h>
#include <xcb/sync.h>
#include <xcb/xfixes.h>
// system
#include <sys/shm.h>
#include <sys/types.h>

namespace KWin
{

namespace Xcb
{

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

QVector<QByteArray> shapeOpCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/xextproto/shape.html
    // extracted from <xcb/shape.h>
    return QVector<QByteArray>({QByteArrayLiteral("QueryVersion"),
                                QByteArrayLiteral("Rectangles"),
                                QByteArrayLiteral("Mask"),
                                QByteArrayLiteral("Combine"),
                                QByteArrayLiteral("Offset"),
                                QByteArrayLiteral("Extents"),
                                QByteArrayLiteral("Input"),
                                QByteArrayLiteral("InputSelected"),
                                QByteArrayLiteral("GetRectangles")});
}

QVector<QByteArray> randrOpCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/randrproto/randrproto.txt
    // extracted from <xcb/randr.h>
    return QVector<QByteArray>({QByteArrayLiteral("QueryVersion"),
                                QByteArray(""), // doesn't exist
                                QByteArrayLiteral("SetScreenConfig"),
                                QByteArray(""), // doesn't exits
                                QByteArrayLiteral("SelectInput"),
                                QByteArrayLiteral("GetScreenInfo"),
                                QByteArrayLiteral("GetScreenSizeRange"),
                                QByteArrayLiteral("SetScreenSize"),
                                QByteArrayLiteral("GetScreenResources"),
                                QByteArrayLiteral("GetOutputInfo"),
                                QByteArrayLiteral("ListOutputProperties"),
                                QByteArrayLiteral("QueryOutputProperty"),
                                QByteArrayLiteral("ConfigureOutputProperty"),
                                QByteArrayLiteral("ChangeOutputProperty"),
                                QByteArrayLiteral("DeleteOutputProperty"),
                                QByteArrayLiteral("GetOutputproperty"),
                                QByteArrayLiteral("CreateMode"),
                                QByteArrayLiteral("DestroyMode"),
                                QByteArrayLiteral("AddOutputMode"),
                                QByteArrayLiteral("DeleteOutputMode"),
                                QByteArrayLiteral("GetCrtcInfo"),
                                QByteArrayLiteral("SetCrtcConfig"),
                                QByteArrayLiteral("GetCrtcGammaSize"),
                                QByteArrayLiteral("GetCrtcGamma"),
                                QByteArrayLiteral("SetCrtcGamma"),
                                QByteArrayLiteral("GetScreenResourcesCurrent"),
                                QByteArrayLiteral("SetCrtcTransform"),
                                QByteArrayLiteral("GetCrtcTransform"),
                                QByteArrayLiteral("GetPanning"),
                                QByteArrayLiteral("SetPanning"),
                                QByteArrayLiteral("SetOutputPrimary"),
                                QByteArrayLiteral("GetOutputPrimary"),
                                QByteArrayLiteral("GetProviders"),
                                QByteArrayLiteral("GetProviderInfo"),
                                QByteArrayLiteral("SetProviderOffloadSink"),
                                QByteArrayLiteral("SetProviderOutputSource"),
                                QByteArrayLiteral("ListProviderProperties"),
                                QByteArrayLiteral("QueryProviderProperty"),
                                QByteArrayLiteral("ConfigureProviderroperty"),
                                QByteArrayLiteral("ChangeProviderProperty"),
                                QByteArrayLiteral("DeleteProviderProperty"),
                                QByteArrayLiteral("GetProviderProperty")});
}

QVector<QByteArray> randrErrorCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/randrproto/randrproto.txt
    // extracted from <xcb/randr.h>
    return QVector<QByteArray>({QByteArrayLiteral("BadOutput"),
                                QByteArrayLiteral("BadCrtc"),
                                QByteArrayLiteral("BadMode"),
                                QByteArrayLiteral("BadProvider")});
}

QVector<QByteArray> damageOpCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/damageproto/damageproto.txt
    // extracted from <xcb/damage.h>
    return QVector<QByteArray>({QByteArrayLiteral("QueryVersion"),
                                QByteArrayLiteral("Create"),
                                QByteArrayLiteral("Destroy"),
                                QByteArrayLiteral("Subtract"),
                                QByteArrayLiteral("Add")});
}

QVector<QByteArray> damageErrorCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/damageproto/damageproto.txt
    // extracted from <xcb/damage.h>
    return QVector<QByteArray>({QByteArrayLiteral("BadDamage")});
}

QVector<QByteArray> compositeOpCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/compositeproto/compositeproto.txt
    // extracted from <xcb/composite.h>
    return QVector<QByteArray>({QByteArrayLiteral("QueryVersion"),
                                QByteArrayLiteral("RedirectWindow"),
                                QByteArrayLiteral("RedirectSubwindows"),
                                QByteArrayLiteral("UnredirectWindow"),
                                QByteArrayLiteral("UnredirectSubwindows"),
                                QByteArrayLiteral("CreateRegionFromBorderClip"),
                                QByteArrayLiteral("NameWindowPixmap"),
                                QByteArrayLiteral("GetOverlayWindow"),
                                QByteArrayLiteral("ReleaseOverlayWindow")});
}

QVector<QByteArray> fixesOpCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/fixesproto/fixesproto.txt
    // extracted from <xcb/xfixes.h>
    return QVector<QByteArray>({QByteArrayLiteral("QueryVersion"),
                                QByteArrayLiteral("ChangeSaveSet"),
                                QByteArrayLiteral("SelectSelectionInput"),
                                QByteArrayLiteral("SelectCursorInput"),
                                QByteArrayLiteral("GetCursorImage"),
                                QByteArrayLiteral("CreateRegion"),
                                QByteArrayLiteral("CreateRegionFromBitmap"),
                                QByteArrayLiteral("CreateRegionFromWindow"),
                                QByteArrayLiteral("CreateRegionFromGc"),
                                QByteArrayLiteral("CreateRegionFromPicture"),
                                QByteArrayLiteral("DestroyRegion"),
                                QByteArrayLiteral("SetRegion"),
                                QByteArrayLiteral("CopyRegion"),
                                QByteArrayLiteral("UnionRegion"),
                                QByteArrayLiteral("IntersectRegion"),
                                QByteArrayLiteral("SubtractRegion"),
                                QByteArrayLiteral("InvertRegion"),
                                QByteArrayLiteral("TranslateRegion"),
                                QByteArrayLiteral("RegionExtents"),
                                QByteArrayLiteral("FetchRegion"),
                                QByteArrayLiteral("SetGcClipRegion"),
                                QByteArrayLiteral("SetWindowShapeRegion"),
                                QByteArrayLiteral("SetPictureClipRegion"),
                                QByteArrayLiteral("SetCursorName"),
                                QByteArrayLiteral("GetCursorName"),
                                QByteArrayLiteral("GetCursorImageAndName"),
                                QByteArrayLiteral("ChangeCursor"),
                                QByteArrayLiteral("ChangeCursorByName"),
                                QByteArrayLiteral("ExpandRegion"),
                                QByteArrayLiteral("HideCursor"),
                                QByteArrayLiteral("ShowCursor"),
                                QByteArrayLiteral("CreatePointerBarrier"),
                                QByteArrayLiteral("DeletePointerBarrier")});
}

QVector<QByteArray> fixesErrorCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/fixesproto/fixesproto.txt
    // extracted from <xcb/xfixes.h>
    return QVector<QByteArray>({QByteArrayLiteral("BadRegion")});
}

QVector<QByteArray> renderOpCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/renderproto/renderproto.txt
    // extracted from <xcb/render.h>
    return QVector<QByteArray>({QByteArrayLiteral("QueryVersion"),
                                QByteArrayLiteral("QueryPictFormats"),
                                QByteArrayLiteral("QueryPictIndexValues"),
                                QByteArrayLiteral("CreatePicture"),
                                QByteArrayLiteral("ChangePicture"),
                                QByteArrayLiteral("SetPictureClipRectangles"),
                                QByteArrayLiteral("FreePicture"),
                                QByteArrayLiteral("Composite"),
                                QByteArrayLiteral("Trapezoids"),
                                QByteArrayLiteral("Triangles"),
                                QByteArrayLiteral("TriStrip"),
                                QByteArrayLiteral("TriFan"),
                                QByteArrayLiteral("CreateGlyphSet"),
                                QByteArrayLiteral("ReferenceGlyphSet"),
                                QByteArrayLiteral("FreeGlyphSet"),
                                QByteArrayLiteral("AddGlyphs"),
                                QByteArrayLiteral("FreeGlyphs"),
                                QByteArrayLiteral("CompositeGlyphs8"),
                                QByteArrayLiteral("CompositeGlyphs16"),
                                QByteArrayLiteral("CompositeGlyphs32"),
                                QByteArrayLiteral("FillRectangles"),
                                QByteArrayLiteral("CreateCursor"),
                                QByteArrayLiteral("SetPictureTransform"),
                                QByteArrayLiteral("QueryFilters"),
                                QByteArrayLiteral("SetPictureFilter"),
                                QByteArrayLiteral("CreateAnimCursor"),
                                QByteArrayLiteral("AddTraps"),
                                QByteArrayLiteral("CreateSolidFill"),
                                QByteArrayLiteral("CreateLinearGradient"),
                                QByteArrayLiteral("CreateRadialGradient"),
                                QByteArrayLiteral("CreateConicalGradient")});
}

QVector<QByteArray> syncOpCodes()
{
    // see https://www.x.org/releases/X11R7.7/doc/xextproto/sync.html
    // extracted from <xcb/sync.h>
    return QVector<QByteArray>({QByteArrayLiteral("Initialize"),
                                QByteArrayLiteral("ListSystemCounters"),
                                QByteArrayLiteral("CreateCounter"),
                                QByteArrayLiteral("DestroyCounter"),
                                QByteArrayLiteral("QueryCounter"),
                                QByteArrayLiteral("Await"),
                                QByteArrayLiteral("ChangeCounter"),
                                QByteArrayLiteral("SetCounter"),
                                QByteArrayLiteral("CreateAlarm"),
                                QByteArrayLiteral("ChangeAlarm"),
                                QByteArrayLiteral("DestroyAlarm"),
                                QByteArrayLiteral("QueryAlarm"),
                                QByteArrayLiteral("SetPriority"),
                                QByteArrayLiteral("GetPriority"),
                                QByteArrayLiteral("CreateFence"),
                                QByteArrayLiteral("TriggerFence"),
                                QByteArrayLiteral("ResetFence"),
                                QByteArrayLiteral("DestroyFence"),
                                QByteArrayLiteral("QueryFence"),
                                QByteArrayLiteral("AwaitFence")});
}

static QVector<QByteArray> glxOpCodes()
{
    return QVector<QByteArray>{
        QByteArrayLiteral(""),
        QByteArrayLiteral("Render"),
        QByteArrayLiteral("RenderLarge"),
        QByteArrayLiteral("CreateContext"),
        QByteArrayLiteral("DestroyContext"),
        QByteArrayLiteral("MakeCurrent"),
        QByteArrayLiteral("IsDirect"),
        QByteArrayLiteral("QueryVersion"),
        QByteArrayLiteral("WaitGL"),
        QByteArrayLiteral("WaitX"),
        QByteArrayLiteral("CopyContext"),
        QByteArrayLiteral("SwapBuffers"),
        QByteArrayLiteral("UseXFont"),
        QByteArrayLiteral("CreateGLXPixmap"),
        QByteArrayLiteral("GetVisualConfigs"),
        QByteArrayLiteral("DestroyGLXPixmap"),
        QByteArrayLiteral("VendorPrivate"),
        QByteArrayLiteral("VendorPrivateWithReply"),
        QByteArrayLiteral("QueryExtensionsString"),
        QByteArrayLiteral("QueryServerString"),
        QByteArrayLiteral("ClientInfo"),
        QByteArrayLiteral("GetFBConfigs"),
        QByteArrayLiteral("CreatePixmap"),
        QByteArrayLiteral("DestroyPixmap"),
        QByteArrayLiteral("CreateNewContext"),
        QByteArrayLiteral("QueryContext"),
        QByteArrayLiteral("MakeContextCurrent"),
        QByteArrayLiteral("CreatePbuffer"),
        QByteArrayLiteral("DestroyPbuffer"),
        QByteArrayLiteral("GetDrawableAttributes"),
        QByteArrayLiteral("ChangeDrawableAttributes"),
        QByteArrayLiteral("CreateWindow"),
        QByteArrayLiteral("DeleteWindow"),
        QByteArrayLiteral("SetClientInfoARB"),
        QByteArrayLiteral("CreateContextAttribsARB"),
        QByteArrayLiteral("SetClientInfo2ARB")
        // Opcodes 36-100 are unused
        // The GL single commands begin at opcode 101
    };
}

static QVector<QByteArray> glxErrorCodes()
{
    return QVector<QByteArray>{
        QByteArrayLiteral("BadContext"),
        QByteArrayLiteral("BadContextState"),
        QByteArrayLiteral("BadDrawable"),
        QByteArrayLiteral("BadPixmap"),
        QByteArrayLiteral("BadContextTag"),
        QByteArrayLiteral("BadCurrentWindow"),
        QByteArrayLiteral("BadRenderRequest"),
        QByteArrayLiteral("BadLargeRequest"),
        QByteArrayLiteral("UnsupportedPrivateRequest"),
        QByteArrayLiteral("BadFBConfig"),
        QByteArrayLiteral("BadPbuffer"),
        QByteArrayLiteral("BadCurrentDrawable"),
        QByteArrayLiteral("BadWindow"),
        QByteArrayLiteral("GLXBadProfileARB")};
}

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
    UniqueCPtr<reply> version(f(connection(), cookie, nullptr));
    dataToFill->version = version->major_version * 0x10 + version->minor_version;
}

Extensions *Extensions::s_self = nullptr;

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
    s_self = nullptr;
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
    Q_ASSERT(c);
    xcb_prefetch_extension_data(c, &xcb_shape_id);
    xcb_prefetch_extension_data(c, &xcb_randr_id);
    xcb_prefetch_extension_data(c, &xcb_damage_id);
    xcb_prefetch_extension_data(c, &xcb_composite_id);
    xcb_prefetch_extension_data(c, &xcb_xfixes_id);
    xcb_prefetch_extension_data(c, &xcb_render_id);
    xcb_prefetch_extension_data(c, &xcb_sync_id);
    xcb_prefetch_extension_data(c, &xcb_glx_id);

    m_shape.name = QByteArray("SHAPE");
    m_randr.name = QByteArray("RANDR");
    m_damage.name = QByteArray("DAMAGE");
    m_composite.name = QByteArray("Composite");
    m_fixes.name = QByteArray("XFIXES");
    m_render.name = QByteArray("RENDER");
    m_sync.name = QByteArray("SYNC");
    m_glx.name = QByteArray("GLX");

    m_shape.opCodes = shapeOpCodes();
    m_randr.opCodes = randrOpCodes();
    m_damage.opCodes = damageOpCodes();
    m_composite.opCodes = compositeOpCodes();
    m_fixes.opCodes = fixesOpCodes();
    m_render.opCodes = renderOpCodes();
    m_sync.opCodes = syncOpCodes();
    m_glx.opCodes = glxOpCodes();

    m_randr.errorCodes = randrErrorCodes();
    m_damage.errorCodes = damageErrorCodes();
    m_fixes.errorCodes = fixesErrorCodes();
    m_glx.errorCodes = glxErrorCodes();

    extensionQueryReply(xcb_get_extension_data(c, &xcb_shape_id), &m_shape);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_randr_id), &m_randr);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_damage_id), &m_damage);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_composite_id), &m_composite);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_xfixes_id), &m_fixes);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_render_id), &m_render);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_sync_id), &m_sync);
    extensionQueryReply(xcb_get_extension_data(c, &xcb_glx_id), &m_glx);

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
        xcb_randr_select_input(connection(), rootWindow(), XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
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
    qCDebug(KWIN_CORE) << "Extensions: shape: 0x" << QString::number(m_shape.version, 16)
                       << " composite: 0x" << QString::number(m_composite.version, 16)
                       << " render: 0x" << QString::number(m_render.version, 16)
                       << " fixes: 0x" << QString::number(m_fixes.version, 16)
                       << " randr: 0x" << QString::number(m_randr.version, 16)
                       << " sync: 0x" << QString::number(m_sync.version, 16)
                       << " damage: 0x " << QString::number(m_damage.version, 16);
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
    UniqueCPtr<xcb_shape_query_extents_reply_t> extents(xcb_shape_query_extents_reply(
        connection(), xcb_shape_query_extents_unchecked(connection(), w), nullptr));
    if (!extents) {
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

int Extensions::fixesSelectionNotifyEvent() const
{
    return m_fixes.eventBase + XCB_XFIXES_SELECTION_NOTIFY;
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
    return {
        m_shape,
        m_randr,
        m_damage,
        m_composite,
        m_render,
        m_fixes,
        m_sync,
        m_glx};
}

//****************************************
// Shm
//****************************************
Shm::Shm()
    : m_shmId(-1)
    , m_buffer(nullptr)
    , m_segment(XCB_NONE)
    , m_valid(false)
    , m_pixmapFormat(XCB_IMAGE_FORMAT_XY_BITMAP)
{
    m_valid = init();
}

Shm::~Shm()
{
    if (m_valid) {
        xcb_shm_detach(connection(), m_segment);
        shmdt(m_buffer);
    }
}

bool Shm::init()
{
    const xcb_query_extension_reply_t *ext = xcb_get_extension_data(connection(), &xcb_shm_id);
    if (!ext || !ext->present) {
        qCDebug(KWIN_CORE) << "SHM extension not available";
        return false;
    }
    UniqueCPtr<xcb_shm_query_version_reply_t> version(xcb_shm_query_version_reply(connection(),
                                                                                  xcb_shm_query_version_unchecked(connection()), nullptr));
    if (!version) {
        qCDebug(KWIN_CORE) << "Failed to get SHM extension version information";
        return false;
    }
    m_pixmapFormat = version->pixmap_format;
    const int MAXSIZE = 4096 * 2048 * 4; // TODO check there are not larger windows
    m_shmId = shmget(IPC_PRIVATE, MAXSIZE, IPC_CREAT | 0600);
    if (m_shmId < 0) {
        qCDebug(KWIN_CORE) << "Failed to allocate SHM segment";
        return false;
    }
    m_buffer = shmat(m_shmId, nullptr, 0 /*read/write*/);
    if (-1 == reinterpret_cast<long>(m_buffer)) {
        qCDebug(KWIN_CORE) << "Failed to attach SHM segment";
        shmctl(m_shmId, IPC_RMID, nullptr);
        return false;
    }
    shmctl(m_shmId, IPC_RMID, nullptr);

    m_segment = xcb_generate_id(connection());
    const xcb_void_cookie_t cookie = xcb_shm_attach_checked(connection(), m_segment, m_shmId, false);
    UniqueCPtr<xcb_generic_error_t> error(xcb_request_check(connection(), cookie));
    if (error) {
        qCDebug(KWIN_CORE) << "xcb_shm_attach error: " << error->error_code;
        shmdt(m_buffer);
        return false;
    }

    return true;
}

uint32_t toXNative(qreal value)
{
    //debug helper, check for things getting mangled
    if (!qFuzzyIsNull(std::fmod(kwinApp()->xwaylandScale() * value, 1))) {
        qCDebug(KWIN_CORE) << "precision lost! floating value sent to X" << kwinApp()->xwaylandScale() * value;
    }
    return std::round(kwinApp()->xwaylandScale() * value);
}

QRect toXNative(const QRectF &r)
{
    return QRect(toXNative(r.x()), toXNative(r.y()), toXNative(r.width()), toXNative(r.height()));
}

qreal fromXNative(int value)
{
    return value / kwinApp()->xwaylandScale();
}

QRectF fromXNative(const QRect &r)
{
    return QRectF(fromXNative(r.x()), fromXNative(r.y()), fromXNative(r.width()), fromXNative(r.height()));
}

QSizeF fromXNative(const QSize &s)
{
    return QSizeF(fromXNative(s.width()), fromXNative(s.height()));
}

static qreal nativeFloor(qreal value)
{
    return std::floor(value * kwinApp()->xwaylandScale()) / kwinApp()->xwaylandScale();
}

QRectF nativeFloor(const QRectF &rect)
{
    const auto output = workspace()->outputAt(rect.center());
    const QRectF outputRect = output->mapFromGlobal(rect);
    return output->mapToGlobal(QRectF(nativeFloor(outputRect.left()), nativeFloor(outputRect.top()),
                                      nativeFloor(outputRect.width()), nativeFloor(outputRect.height())));
}

} // namespace Xcb
} // namespace KWin

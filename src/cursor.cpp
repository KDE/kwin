/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursor.h"
// kwin
#include "compositor.h"
#include "core/output.h"
#include "cursorsource.h"
#include "input.h"
#include "keyboard_input.h"
#include "main.h"
#include "scene/workspacescene.h"
#include "utils/common.h"

#if KWIN_BUILD_X11
#include "utils/xcbutils.h"
#endif
// KDE
#include <KConfig>
#include <KConfigGroup>
// Qt
#include <QAbstractEventDispatcher>
#include <QDBusConnection>
#include <QScreen>
#include <QTimer>

#include <xcb/xcb_cursor.h>

namespace KWin
{
Cursors *Cursors::s_self = nullptr;
Cursors *Cursors::self()
{
    if (!s_self) {
        s_self = new Cursors;
    }
    return s_self;
}

void Cursors::addCursor(Cursor *cursor)
{
    Q_ASSERT(!m_cursors.contains(cursor));
    m_cursors += cursor;

    connect(cursor, &Cursor::posChanged, this, [this, cursor](const QPointF &pos) {
        setCurrentCursor(cursor);
        Q_EMIT positionChanged(cursor, pos);
    });
}

void Cursors::removeCursor(Cursor *cursor)
{
    m_cursors.removeOne(cursor);
    if (m_currentCursor == cursor) {
        if (m_cursors.isEmpty()) {
            m_currentCursor = nullptr;
        } else {
            setCurrentCursor(m_cursors.constFirst());
        }
    }
    if (m_mouse == cursor) {
        m_mouse = nullptr;
    }
}

void Cursors::hideCursor()
{
    m_cursorHideCounter++;
    if (m_cursorHideCounter == 1) {
        Q_EMIT hiddenChanged();
    }
}

void Cursors::showCursor()
{
    m_cursorHideCounter--;
    if (m_cursorHideCounter == 0) {
        Q_EMIT hiddenChanged();
    }
}

bool Cursors::isCursorHidden() const
{
    return m_cursorHideCounter > 0;
}

void Cursors::setCurrentCursor(Cursor *cursor)
{
    if (m_currentCursor == cursor) {
        return;
    }

    Q_ASSERT(m_cursors.contains(cursor) || !cursor);

    if (m_currentCursor) {
        disconnect(m_currentCursor, &Cursor::cursorChanged, this, &Cursors::emitCurrentCursorChanged);
    }
    m_currentCursor = cursor;
    connect(m_currentCursor, &Cursor::cursorChanged, this, &Cursors::emitCurrentCursorChanged);

    Q_EMIT currentCursorChanged(m_currentCursor);
}

void Cursors::emitCurrentCursorChanged()
{
    Q_EMIT currentCursorChanged(m_currentCursor);
}

Cursor::Cursor()
    : m_themeName(defaultThemeName())
    , m_themeSize(defaultThemeSize())
{
    loadThemeSettings();
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"),
                                          QStringLiteral("notifyChange"), this, SLOT(slotKGlobalSettingsNotifyChange(int, int)));

    connect(kwinApp(), &Application::x11ConnectionChanged, this, [this]() {
        m_cursors.clear();
    });
}

Cursor::~Cursor()
{
    Cursors::self()->removeCursor(this);
}

void Cursor::loadThemeSettings()
{
    QString themeName = QString::fromUtf8(qgetenv("XCURSOR_THEME"));
    bool ok = false;
    // XCURSOR_SIZE might not be set (e.g. by startkde)
    const uint themeSize = qEnvironmentVariableIntValue("XCURSOR_SIZE", &ok);
    if (!themeName.isEmpty() && ok) {
        updateTheme(themeName, themeSize);
        return;
    }
    // didn't get from environment variables, read from config file
    loadThemeFromKConfig();
}

void Cursor::loadThemeFromKConfig()
{
    KConfigGroup mousecfg(kwinApp()->inputConfig(), QStringLiteral("Mouse"));
    const QString themeName = mousecfg.readEntry("cursorTheme", defaultThemeName());
    const uint themeSize = mousecfg.readEntry("cursorSize", defaultThemeSize());
    updateTheme(themeName, themeSize);
}

void Cursor::updateTheme(const QString &name, int size)
{
    if (m_themeName != name || m_themeSize != size) {
        m_themeName = name;
        m_themeSize = size;
        m_cursors.clear();
        Q_EMIT themeChanged();
    }
}

void Cursor::slotKGlobalSettingsNotifyChange(int type, int arg)
{
    if (type == 5 /*CursorChanged*/) {
        kwinApp()->inputConfig()->reparseConfiguration();
        loadThemeFromKConfig();
    }
}

bool Cursor::isOnOutput(Output *output) const
{
    if (Cursors::self()->isCursorHidden()) {
        return false;
    }
    return geometry().intersects(output->geometry());
}

QPointF Cursor::hotspot() const
{
    if (Q_UNLIKELY(!m_source)) {
        return QPointF();
    }
    return m_source->hotspot();
}

QRectF Cursor::geometry() const
{
    return rect().translated(m_pos - hotspot());
}

QRectF Cursor::rect() const
{
    if (Q_UNLIKELY(!m_source)) {
        return QRectF();
    } else {
        return QRectF(QPointF(0, 0), m_source->size());
    }
}

QPointF Cursor::pos()
{
    doGetPos();
    return m_pos;
}

void Cursor::setPos(const QPointF &pos)
{
    // first query the current pos to not warp to the already existing pos
    if (pos == m_pos) {
        return;
    }
    m_pos = pos;
    doSetPos();
}

#if KWIN_BUILD_X11
xcb_cursor_t Cursor::x11Cursor(CursorShape shape)
{
    return x11Cursor(shape.name());
}

xcb_cursor_t Cursor::x11Cursor(const QByteArray &name)
{
    Q_ASSERT(kwinApp()->x11Connection());
    auto it = m_cursors.constFind(name);
    if (it != m_cursors.constEnd()) {
        return it.value();
    }

    if (name.isEmpty()) {
        return XCB_CURSOR_NONE;
    }

    xcb_cursor_context_t *ctx;
    if (xcb_cursor_context_new(kwinApp()->x11Connection(), Xcb::defaultScreen(), &ctx) < 0) {
        return XCB_CURSOR_NONE;
    }

    xcb_cursor_t cursor = xcb_cursor_load_cursor(ctx, name.constData());
    if (cursor == XCB_CURSOR_NONE) {
        const auto &names = CursorShape::alternatives(name);
        for (const QByteArray &cursorName : names) {
            cursor = xcb_cursor_load_cursor(ctx, cursorName.constData());
            if (cursor != XCB_CURSOR_NONE) {
                break;
            }
        }
    }
    if (cursor != XCB_CURSOR_NONE) {
        m_cursors.insert(name, cursor);
    }

    xcb_cursor_context_free(ctx);
    return cursor;
}
#endif

void Cursor::doSetPos()
{
    Q_EMIT posChanged(m_pos);
}

void Cursor::doGetPos()
{
}

void Cursor::updatePos(const QPointF &pos)
{
    if (m_pos == pos) {
        return;
    }
    m_pos = pos;
    Q_EMIT posChanged(m_pos);
}

QString Cursor::defaultThemeName()
{
    return QStringLiteral("default");
}

int Cursor::defaultThemeSize()
{
    return 24;
}

QString Cursor::fallbackThemeName()
{
    return QStringLiteral("breeze_cursors");
}

QList<QByteArray> CursorShape::alternatives(const QByteArray &name)
{
    static const QHash<QByteArray, QList<QByteArray>> alternatives = {
        {
            QByteArrayLiteral("crosshair"),
            {
                QByteArrayLiteral("cross"),
                QByteArrayLiteral("diamond-cross"),
                QByteArrayLiteral("cross-reverse"),
            },
        },
        {
            QByteArrayLiteral("default"),
            {
                QByteArrayLiteral("left_ptr"),
                QByteArrayLiteral("arrow"),
                QByteArrayLiteral("dnd-none"),
                QByteArrayLiteral("op_left_arrow"),
            },
        },
        {
            QByteArrayLiteral("up-arrow"),
            {
                QByteArrayLiteral("up_arrow"),
                QByteArrayLiteral("sb_up_arrow"),
                QByteArrayLiteral("center_ptr"),
                QByteArrayLiteral("centre_ptr"),
            },
        },
        {
            QByteArrayLiteral("wait"),
            {
                QByteArrayLiteral("watch"),
                QByteArrayLiteral("progress"),
            },
        },
        {
            QByteArrayLiteral("text"),
            {
                QByteArrayLiteral("ibeam"),
                QByteArrayLiteral("xterm"),
            },
        },
        {
            QByteArrayLiteral("all-scroll"),
            {
                QByteArrayLiteral("size_all"),
                QByteArrayLiteral("fleur"),
            },
        },
        {
            QByteArrayLiteral("pointer"),
            {
                QByteArrayLiteral("pointing_hand"),
                QByteArrayLiteral("hand2"),
                QByteArrayLiteral("hand"),
                QByteArrayLiteral("hand1"),
                QByteArrayLiteral("e29285e634086352946a0e7090d73106"),
                QByteArrayLiteral("9d800788f1b08800ae810202380a0822"),
            },
        },
        {
            QByteArrayLiteral("ns-resize"),
            {
                QByteArrayLiteral("size_ver"),
                QByteArrayLiteral("00008160000006810000408080010102"),
                QByteArrayLiteral("sb_v_double_arrow"),
                QByteArrayLiteral("v_double_arrow"),
                QByteArrayLiteral("n-resize"),
                QByteArrayLiteral("s-resize"),
                QByteArrayLiteral("col-resize"),
                QByteArrayLiteral("top_side"),
                QByteArrayLiteral("bottom_side"),
                QByteArrayLiteral("base_arrow_up"),
                QByteArrayLiteral("base_arrow_down"),
                QByteArrayLiteral("based_arrow_down"),
                QByteArrayLiteral("based_arrow_up"),
            },
        },
        {
            QByteArrayLiteral("ew-resize"),
            {
                QByteArrayLiteral("size_hor"),
                QByteArrayLiteral("028006030e0e7ebffc7f7070c0600140"),
                QByteArrayLiteral("sb_h_double_arrow"),
                QByteArrayLiteral("h_double_arrow"),
                QByteArrayLiteral("e-resize"),
                QByteArrayLiteral("w-resize"),
                QByteArrayLiteral("row-resize"),
                QByteArrayLiteral("right_side"),
                QByteArrayLiteral("left_side"),
            },
        },
        {
            QByteArrayLiteral("nesw-resize"),
            {
                QByteArrayLiteral("size_bdiag"),
                QByteArrayLiteral("fcf1c3c7cd4491d801f1e1c78f100000"),
                QByteArrayLiteral("fd_double_arrow"),
                QByteArrayLiteral("bottom_left_corner"),
                QByteArrayLiteral("top_right_corner"),
            },
        },
        {
            QByteArrayLiteral("nwse-resize"),
            {
                QByteArrayLiteral("size_fdiag"),
                QByteArrayLiteral("c7088f0f3e6c8088236ef8e1e3e70000"),
                QByteArrayLiteral("bd_double_arrow"),
                QByteArrayLiteral("bottom_right_corner"),
                QByteArrayLiteral("top_left_corner"),
            },
        },
        {
            QByteArrayLiteral("help"),
            {
                QByteArrayLiteral("whats_this"),
                QByteArrayLiteral("d9ce0ab605698f320427677b458ad60b"),
                QByteArrayLiteral("left_ptr_help"),
                QByteArrayLiteral("question_arrow"),
                QByteArrayLiteral("dnd-ask"),
                QByteArrayLiteral("5c6cd98b3f3ebcb1f9c7f1c204630408"),
            },
        },
        {
            QByteArrayLiteral("col-resize"),
            {
                QByteArrayLiteral("split_h"),
                QByteArrayLiteral("14fef782d02440884392942c11205230"),
                QByteArrayLiteral("size_hor"),
            },
        },
        {
            QByteArrayLiteral("row-resize"),
            {
                QByteArrayLiteral("split_v"),
                QByteArrayLiteral("2870a09082c103050810ffdffffe0204"),
                QByteArrayLiteral("size_ver"),
            },
        },
        {
            QByteArrayLiteral("not-allowed"),
            {
                QByteArrayLiteral("forbidden"),
                QByteArrayLiteral("03b6e0fcb3499374a867c041f52298f0"),
                QByteArrayLiteral("circle"),
                QByteArrayLiteral("dnd-no-drop"),
            },
        },
        {
            QByteArrayLiteral("progress"),
            {
                QByteArrayLiteral("left_ptr_watch"),
                QByteArrayLiteral("3ecb610c1bf2410f44200f48c40d3599"),
                QByteArrayLiteral("00000000000000020006000e7e9ffc3f"),
                QByteArrayLiteral("08e8e1c95fe2fc01f976f1e063a24ccd"),
            },
        },
        {
            QByteArrayLiteral("grab"),
            {
                QByteArrayLiteral("openhand"),
                QByteArrayLiteral("9141b49c8149039304290b508d208c40"),
                QByteArrayLiteral("all_scroll"),
                QByteArrayLiteral("all-scroll"),
            },
        },
        {
            QByteArrayLiteral("grabbing"),
            {
                QByteArrayLiteral("closedhand"),
                QByteArrayLiteral("05e88622050804100c20044008402080"),
                QByteArrayLiteral("4498f0e0c1937ffe01fd06f973665830"),
                QByteArrayLiteral("9081237383d90e509aa00f00170e968f"),
                QByteArrayLiteral("fcf21c00b30f7e3f83fe0dfd12e71cff"),
            },
        },
        {
            QByteArrayLiteral("alias"),
            {
                QByteArrayLiteral("link"),
                QByteArrayLiteral("dnd-link"),
                QByteArrayLiteral("3085a0e285430894940527032f8b26df"),
                QByteArrayLiteral("640fb0e74195791501fd1ed57b41487f"),
                QByteArrayLiteral("a2a266d0498c3104214a47bd64ab0fc8"),
            },
        },
        {
            QByteArrayLiteral("copy"),
            {
                QByteArrayLiteral("dnd-copy"),
                QByteArrayLiteral("1081e37283d90000800003c07f3ef6bf"),
                QByteArrayLiteral("6407b0e94181790501fd1e167b474872"),
                QByteArrayLiteral("b66166c04f8c3109214a4fbd64a50fc8"),
            },
        },
        {
            QByteArrayLiteral("move"),
            {
                QByteArrayLiteral("dnd-move"),
            },
        },
        {
            QByteArrayLiteral("sw-resize"),
            {
                QByteArrayLiteral("size_bdiag"),
                QByteArrayLiteral("fcf1c3c7cd4491d801f1e1c78f100000"),
                QByteArrayLiteral("fd_double_arrow"),
                QByteArrayLiteral("bottom_left_corner"),
            },
        },
        {
            QByteArrayLiteral("se-resize"),
            {
                QByteArrayLiteral("size_fdiag"),
                QByteArrayLiteral("c7088f0f3e6c8088236ef8e1e3e70000"),
                QByteArrayLiteral("bd_double_arrow"),
                QByteArrayLiteral("bottom_right_corner"),
            },
        },
        {
            QByteArrayLiteral("ne-resize"),
            {
                QByteArrayLiteral("size_bdiag"),
                QByteArrayLiteral("fcf1c3c7cd4491d801f1e1c78f100000"),
                QByteArrayLiteral("fd_double_arrow"),
                QByteArrayLiteral("top_right_corner"),
            },
        },
        {
            QByteArrayLiteral("nw-resize"),
            {
                QByteArrayLiteral("size_fdiag"),
                QByteArrayLiteral("c7088f0f3e6c8088236ef8e1e3e70000"),
                QByteArrayLiteral("bd_double_arrow"),
                QByteArrayLiteral("top_left_corner"),
            },
        },
        {
            QByteArrayLiteral("n-resize"),
            {
                QByteArrayLiteral("size_ver"),
                QByteArrayLiteral("00008160000006810000408080010102"),
                QByteArrayLiteral("sb_v_double_arrow"),
                QByteArrayLiteral("v_double_arrow"),
                QByteArrayLiteral("col-resize"),
                QByteArrayLiteral("top_side"),
            },
        },
        {
            QByteArrayLiteral("e-resize"),
            {
                QByteArrayLiteral("size_hor"),
                QByteArrayLiteral("028006030e0e7ebffc7f7070c0600140"),
                QByteArrayLiteral("sb_h_double_arrow"),
                QByteArrayLiteral("h_double_arrow"),
                QByteArrayLiteral("row-resize"),
                QByteArrayLiteral("left_side"),
            },
        },
        {
            QByteArrayLiteral("s-resize"),
            {
                QByteArrayLiteral("size_ver"),
                QByteArrayLiteral("00008160000006810000408080010102"),
                QByteArrayLiteral("sb_v_double_arrow"),
                QByteArrayLiteral("v_double_arrow"),
                QByteArrayLiteral("col-resize"),
                QByteArrayLiteral("bottom_side"),
            },
        },
        {
            QByteArrayLiteral("w-resize"),
            {
                QByteArrayLiteral("size_hor"),
                QByteArrayLiteral("028006030e0e7ebffc7f7070c0600140"),
                QByteArrayLiteral("sb_h_double_arrow"),
                QByteArrayLiteral("h_double_arrow"),
                QByteArrayLiteral("right_side"),
            },
        },
    };
    auto it = alternatives.find(name);
    if (it != alternatives.end()) {
        return it.value();
    }
    return QList<QByteArray>();
}

QByteArray CursorShape::name() const
{
    switch (m_shape) {
    case Qt::ArrowCursor:
        return QByteArrayLiteral("default");
    case Qt::UpArrowCursor:
        return QByteArrayLiteral("up-arrow");
    case Qt::CrossCursor:
        return QByteArrayLiteral("crosshair");
    case Qt::WaitCursor:
        return QByteArrayLiteral("wait");
    case Qt::IBeamCursor:
        return QByteArrayLiteral("text");
    case Qt::SizeVerCursor:
        return QByteArrayLiteral("ns-resize");
    case Qt::SizeHorCursor:
        return QByteArrayLiteral("ew-resize");
    case Qt::SizeBDiagCursor:
        return QByteArrayLiteral("nesw-resize");
    case Qt::SizeFDiagCursor:
        return QByteArrayLiteral("nwse-resize");
    case Qt::SizeAllCursor:
        return QByteArrayLiteral("all-scroll");
    case Qt::SplitVCursor:
        return QByteArrayLiteral("row-resize");
    case Qt::SplitHCursor:
        return QByteArrayLiteral("col-resize");
    case Qt::PointingHandCursor:
        return QByteArrayLiteral("pointer");
    case Qt::ForbiddenCursor:
        return QByteArrayLiteral("not-allowed");
    case Qt::OpenHandCursor:
        return QByteArrayLiteral("grab");
    case Qt::ClosedHandCursor:
        return QByteArrayLiteral("grabbing");
    case Qt::WhatsThisCursor:
        return QByteArrayLiteral("help");
    case Qt::BusyCursor:
        return QByteArrayLiteral("progress");
    case Qt::DragMoveCursor:
        return QByteArrayLiteral("move");
    case Qt::DragCopyCursor:
        return QByteArrayLiteral("copy");
    case Qt::DragLinkCursor:
        return QByteArrayLiteral("alias");
    case KWin::ExtendedCursor::SizeNorthEast:
        return QByteArrayLiteral("ne-resize");
    case KWin::ExtendedCursor::SizeNorth:
        return QByteArrayLiteral("n-resize");
    case KWin::ExtendedCursor::SizeNorthWest:
        return QByteArrayLiteral("nw-resize");
    case KWin::ExtendedCursor::SizeEast:
        return QByteArrayLiteral("e-resize");
    case KWin::ExtendedCursor::SizeWest:
        return QByteArrayLiteral("w-resize");
    case KWin::ExtendedCursor::SizeSouthEast:
        return QByteArrayLiteral("se-resize");
    case KWin::ExtendedCursor::SizeSouth:
        return QByteArrayLiteral("s-resize");
    case KWin::ExtendedCursor::SizeSouthWest:
        return QByteArrayLiteral("sw-resize");
    default:
        return QByteArray();
    }
}

CursorSource *Cursor::source() const
{
    return m_source;
}

void Cursor::setSource(CursorSource *source)
{
    if (m_source == source) {
        return;
    }
    if (m_source) {
        disconnect(m_source, &CursorSource::changed, this, &Cursor::cursorChanged);
    }
    m_source = source;
    if (m_source) {
        connect(m_source, &CursorSource::changed, this, &Cursor::cursorChanged);
    }
    Q_EMIT cursorChanged();
}

} // namespace

#include "moc_cursor.cpp"

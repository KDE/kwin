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

#include "cursor.h"
// kwin
#include <kwinglobals.h>
#include "input.h"
#include "keyboard_input.h"
#include "main.h"
#include "platform.h"
#include "utils.h"
#include "xcbutils.h"
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>
// Qt
#include <QAbstractEventDispatcher>
#include <QDBusConnection>
#include <QScreen>
#include <QTimer>
// xcb
#include <xcb/xfixes.h>

namespace KWin
{
Cursor *Cursor::s_self = nullptr;

Cursor::Cursor(QObject *parent)
    : QObject(parent)
    , m_mousePollingCounter(0)
    , m_cursorTrackingCounter(0)
    , m_themeName("default")
    , m_themeSize(24)
{
    s_self = this;
    loadThemeSettings();
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"),
                                          QStringLiteral("notifyChange"), this, SLOT(slotKGlobalSettingsNotifyChange(int,int)));
}

Cursor::~Cursor()
{
    s_self = NULL;
}

void Cursor::loadThemeSettings()
{
    QString themeName = QString::fromUtf8(qgetenv("XCURSOR_THEME"));
    bool ok = false;
    // XCURSOR_SIZE might not be set (e.g. by startkde)
    uint themeSize = 0;
    if (qEnvironmentVariableIsSet("XCURSOR_SIZE")) {
        themeSize = qgetenv("XCURSOR_SIZE").toUInt(&ok);
    }
    if (!ok) {
        if (QScreen *s = QGuiApplication::primaryScreen()) {
            themeSize = s->logicalDotsPerInchY() * 16 / 72;
            ok = true;
        }
    }
    if (!themeName.isEmpty() && ok) {
        updateTheme(themeName, themeSize);
        return;
    }
    // didn't get from environment variables, read from config file
    loadThemeFromKConfig();
}

void Cursor::loadThemeFromKConfig()
{
    KConfigGroup mousecfg(kwinApp()->inputConfig(), "Mouse");
    const QString themeName = mousecfg.readEntry("cursorTheme", "default");
    bool ok = false;
    uint themeSize = mousecfg.readEntry("cursorSize", QString("24")).toUInt(&ok);
    if (!ok) {
        themeSize = 24;
    }
    updateTheme(themeName, themeSize);
}

void Cursor::updateTheme(const QString &name, int size)
{
    if (m_themeName != name || m_themeSize != size) {
        m_themeName = name;
        m_themeSize = size;
        emit themeChanged();
    }
}

void Cursor::slotKGlobalSettingsNotifyChange(int type, int arg)
{
    Q_UNUSED(arg)
    if (type == 5 /*CursorChanged*/) {
        loadThemeFromKConfig();
        // sync to environment
        qputenv("XCURSOR_THEME", m_themeName.toUtf8());
        qputenv("XCURSOR_SIZE", QByteArray::number(m_themeSize));
    }
}

QPoint Cursor::pos()
{
    s_self->doGetPos();
    return s_self->m_pos;
}

void Cursor::setPos(const QPoint &pos)
{
    // first query the current pos to not warp to the already existing pos
    if (pos == Cursor::pos()) {
        return;
    }
    s_self->m_pos = pos;
    s_self->doSetPos();
}

void Cursor::setPos(int x, int y)
{
    Cursor::setPos(QPoint(x, y));
}

xcb_cursor_t Cursor::getX11Cursor(Qt::CursorShape shape)
{
    Q_UNUSED(shape)
    return XCB_CURSOR_NONE;
}

xcb_cursor_t Cursor::getX11Cursor(const QByteArray &name)
{
    Q_UNUSED(name)
    return XCB_CURSOR_NONE;
}

xcb_cursor_t Cursor::x11Cursor(Qt::CursorShape shape)
{
    return s_self->getX11Cursor(shape);
}

xcb_cursor_t Cursor::x11Cursor(const QByteArray &name)
{
    return s_self->getX11Cursor(name);
}

void Cursor::doSetPos()
{
    emit posChanged(m_pos);
}

void Cursor::doGetPos()
{
}

void Cursor::updatePos(const QPoint &pos)
{
    if (m_pos == pos) {
        return;
    }
    m_pos = pos;
    emit posChanged(m_pos);
}

void Cursor::startMousePolling()
{
    ++m_mousePollingCounter;
    if (m_mousePollingCounter == 1) {
        doStartMousePolling();
    }
}

void Cursor::stopMousePolling()
{
    Q_ASSERT(m_mousePollingCounter > 0);
    --m_mousePollingCounter;
    if (m_mousePollingCounter == 0) {
        doStopMousePolling();
    }
}

void Cursor::doStartMousePolling()
{
}

void Cursor::doStopMousePolling()
{
}

void Cursor::startCursorTracking()
{
    ++m_cursorTrackingCounter;
    if (m_cursorTrackingCounter == 1) {
        doStartCursorTracking();
    }
}

void Cursor::stopCursorTracking()
{
    Q_ASSERT(m_cursorTrackingCounter > 0);
    --m_cursorTrackingCounter;
    if (m_cursorTrackingCounter == 0) {
        doStopCursorTracking();
    }
}

void Cursor::doStartCursorTracking()
{
}

void Cursor::doStopCursorTracking()
{
}

void Cursor::notifyCursorChanged(uint32_t serial)
{
    Q_UNUSED(serial)
    if (m_cursorTrackingCounter <= 0) {
        // cursor change tracking is currently disabled, so don't emit signal
        return;
    }
    emit cursorChanged();
}

QVector<QByteArray> Cursor::cursorAlternativeNames(const QByteArray &name) const
{
    static const QHash<QByteArray, QVector<QByteArray>> alternatives = {
        {QByteArrayLiteral("left_ptr"),       {QByteArrayLiteral("arrow"),
                                                QByteArrayLiteral("dnd-none"),
                                                QByteArrayLiteral("op_left_arrow")}},
        {QByteArrayLiteral("cross"),          {QByteArrayLiteral("crosshair"),
                                                QByteArrayLiteral("diamond-cross"),
                                                QByteArrayLiteral("cross-reverse")}},
        {QByteArrayLiteral("up_arrow"),       {QByteArrayLiteral("center_ptr"),
                                                QByteArrayLiteral("sb_up_arrow"),
                                                QByteArrayLiteral("centre_ptr")}},
        {QByteArrayLiteral("wait"),           {QByteArrayLiteral("watch"),
                                                QByteArrayLiteral("progress")}},
        {QByteArrayLiteral("ibeam"),          {QByteArrayLiteral("xterm"),
                                                QByteArrayLiteral("text")}},
        {QByteArrayLiteral("size_all"),       {QByteArrayLiteral("fleur")}},
        {QByteArrayLiteral("pointing_hand"),  {QByteArrayLiteral("hand2"),
                                                QByteArrayLiteral("hand"),
                                                QByteArrayLiteral("hand1"),
                                                QByteArrayLiteral("pointer"),
                                                QByteArrayLiteral("e29285e634086352946a0e7090d73106"),
                                                QByteArrayLiteral("9d800788f1b08800ae810202380a0822")}},
        {QByteArrayLiteral("size_ver"),       {QByteArrayLiteral("00008160000006810000408080010102"),
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
                                                QByteArrayLiteral("based_arrow_up")}},
        {QByteArrayLiteral("size_hor"),       {QByteArrayLiteral("028006030e0e7ebffc7f7070c0600140"),
                                                QByteArrayLiteral("sb_h_double_arrow"),
                                                QByteArrayLiteral("h_double_arrow"),
                                                QByteArrayLiteral("e-resize"),
                                                QByteArrayLiteral("w-resize"),
                                                QByteArrayLiteral("row-resize"),
                                                QByteArrayLiteral("right_side"),
                                                QByteArrayLiteral("left_side")}},
        {QByteArrayLiteral("size_bdiag"),     {QByteArrayLiteral("fcf1c3c7cd4491d801f1e1c78f100000"),
                                                QByteArrayLiteral("fd_double_arrow"),
                                                QByteArrayLiteral("bottom_left_corner"),
                                                QByteArrayLiteral("top_right_corner")}},
        {QByteArrayLiteral("size_fdiag"),     {QByteArrayLiteral("c7088f0f3e6c8088236ef8e1e3e70000"),
                                                QByteArrayLiteral("bd_double_arrow"),
                                                QByteArrayLiteral("bottom_right_corner"),
                                                QByteArrayLiteral("top_left_corner")}},
        {QByteArrayLiteral("whats_this"),     {QByteArrayLiteral("d9ce0ab605698f320427677b458ad60b"),
                                                QByteArrayLiteral("left_ptr_help"),
                                                QByteArrayLiteral("help"),
                                                QByteArrayLiteral("question_arrow"),
                                                QByteArrayLiteral("dnd-ask"),
                                                QByteArrayLiteral("5c6cd98b3f3ebcb1f9c7f1c204630408")}},
        {QByteArrayLiteral("split_h"),        {QByteArrayLiteral("14fef782d02440884392942c11205230"),
                                                QByteArrayLiteral("size_hor")}},
        {QByteArrayLiteral("split_v"),        {QByteArrayLiteral("2870a09082c103050810ffdffffe0204"),
                                                QByteArrayLiteral("size_ver")}},
        {QByteArrayLiteral("forbidden"),      {QByteArrayLiteral("03b6e0fcb3499374a867c041f52298f0"),
                                                QByteArrayLiteral("circle"),
                                                QByteArrayLiteral("dnd-no-drop"),
                                                QByteArrayLiteral("not-allowed")}},
        {QByteArrayLiteral("left_ptr_watch"), {QByteArrayLiteral("3ecb610c1bf2410f44200f48c40d3599"),
                                                QByteArrayLiteral("00000000000000020006000e7e9ffc3f"),
                                                QByteArrayLiteral("08e8e1c95fe2fc01f976f1e063a24ccd")}},
        {QByteArrayLiteral("openhand"),       {QByteArrayLiteral("9141b49c8149039304290b508d208c40"),
                                                QByteArrayLiteral("all_scroll"),
                                                QByteArrayLiteral("all-scroll")}},
        {QByteArrayLiteral("closedhand"),     {QByteArrayLiteral("05e88622050804100c20044008402080"),
                                                QByteArrayLiteral("4498f0e0c1937ffe01fd06f973665830"),
                                                QByteArrayLiteral("9081237383d90e509aa00f00170e968f"),
                                                QByteArrayLiteral("fcf21c00b30f7e3f83fe0dfd12e71cff")}},
        {QByteArrayLiteral("dnd-link"),       {QByteArrayLiteral("link"),
                                                QByteArrayLiteral("alias"),
                                                QByteArrayLiteral("3085a0e285430894940527032f8b26df"),
                                                QByteArrayLiteral("640fb0e74195791501fd1ed57b41487f"),
                                                QByteArrayLiteral("a2a266d0498c3104214a47bd64ab0fc8")}},
        {QByteArrayLiteral("dnd-copy"),       {QByteArrayLiteral("copy"),
                                                QByteArrayLiteral("1081e37283d90000800003c07f3ef6bf"),
                                                QByteArrayLiteral("6407b0e94181790501fd1e167b474872"),
                                                QByteArrayLiteral("b66166c04f8c3109214a4fbd64a50fc8")}},
        {QByteArrayLiteral("dnd-move"),       {QByteArrayLiteral("move")}}
    };
    auto it = alternatives.find(name);
    if (it != alternatives.end()) {
        return it.value();
    }
    return QVector<QByteArray>();
}

QByteArray Cursor::cursorName(Qt::CursorShape shape) const
{
    switch (shape) {
    case Qt::ArrowCursor:
        return QByteArray("left_ptr");
    case Qt::UpArrowCursor:
        return QByteArray("up_arrow");
    case Qt::CrossCursor:
        return QByteArray("cross");
    case Qt::WaitCursor:
        return QByteArray("wait");
    case Qt::IBeamCursor:
        return QByteArray("ibeam");
    case Qt::SizeVerCursor:
        return QByteArray("size_ver");
    case Qt::SizeHorCursor:
        return QByteArray("size_hor");
    case Qt::SizeBDiagCursor:
        return QByteArray("size_bdiag");
    case Qt::SizeFDiagCursor:
        return QByteArray("size_fdiag");
    case Qt::SizeAllCursor:
        return QByteArray("size_all");
    case Qt::SplitVCursor:
        return QByteArray("split_v");
    case Qt::SplitHCursor:
        return QByteArray("split_h");
    case Qt::PointingHandCursor:
        return QByteArray("pointing_hand");
    case Qt::ForbiddenCursor:
        return QByteArray("forbidden");
    case Qt::OpenHandCursor:
        return QByteArray("openhand");
    case Qt::ClosedHandCursor:
        return QByteArray("closedhand");
    case Qt::WhatsThisCursor:
        return QByteArray("whats_this");
    case Qt::BusyCursor:
        return QByteArray("left_ptr_watch");
    case Qt::DragMoveCursor:
        return QByteArray("dnd-move");
    case Qt::DragCopyCursor:
        return QByteArray("dnd-copy");
    case Qt::DragLinkCursor:
        return QByteArray("dnd-link");
    default:
        return QByteArray();
    }
}

InputRedirectionCursor::InputRedirectionCursor(QObject *parent)
    : Cursor(parent)
    , m_currentButtons(Qt::NoButton)
{
    connect(input(), SIGNAL(globalPointerChanged(QPointF)), SLOT(slotPosChanged(QPointF)));
    connect(input(), SIGNAL(pointerButtonStateChanged(uint32_t,InputRedirection::PointerButtonState)),
            SLOT(slotPointerButtonChanged()));
#ifndef KCMRULES
    connect(input(), &InputRedirection::keyboardModifiersChanged,
            this, &InputRedirectionCursor::slotModifiersChanged);
#endif
}

InputRedirectionCursor::~InputRedirectionCursor()
{
}

void InputRedirectionCursor::doSetPos()
{
    if (input()->supportsPointerWarping()) {
        input()->warpPointer(currentPos());
    }
    slotPosChanged(input()->globalPointer());
    emit posChanged(currentPos());
}

void InputRedirectionCursor::slotPosChanged(const QPointF &pos)
{
    const QPoint oldPos = currentPos();
    updatePos(pos.toPoint());
    emit mouseChanged(pos.toPoint(), oldPos, m_currentButtons, m_currentButtons,
                      input()->keyboardModifiers(), input()->keyboardModifiers());
}

void InputRedirectionCursor::slotModifiersChanged(Qt::KeyboardModifiers mods, Qt::KeyboardModifiers oldMods)
{
    emit mouseChanged(currentPos(), currentPos(), m_currentButtons, m_currentButtons, mods, oldMods);
}

void InputRedirectionCursor::slotPointerButtonChanged()
{
    const Qt::MouseButtons oldButtons = m_currentButtons;
    m_currentButtons = input()->qtButtonStates();
    const QPoint pos = currentPos();
    emit mouseChanged(pos, pos, m_currentButtons, oldButtons, input()->keyboardModifiers(), input()->keyboardModifiers());
}

void InputRedirectionCursor::doStartCursorTracking()
{
#ifndef KCMRULES
    connect(kwinApp()->platform(), &Platform::cursorChanged, this, &Cursor::cursorChanged);
#endif
}

void InputRedirectionCursor::doStopCursorTracking()
{
#ifndef KCMRULES
    disconnect(kwinApp()->platform(), &Platform::cursorChanged, this, &Cursor::cursorChanged);
#endif
}

} // namespace

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 This file is for (very) small utility functions/classes.

*/

#include "utils.h"

#include <QWidget>
#include <kkeyserver.h>

#ifndef KCMRULES
#include <QApplication>
#include <QDebug>

#include "atoms.h"
#include "platform.h"
#include "workspace.h"

#include <csignal>
#include <cstdio>

#endif

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtCriticalMsg)
Q_LOGGING_CATEGORY(KWIN_VIRTUALKEYBOARD, "kwin_virtualkeyboard", QtCriticalMsg)
namespace KWin
{

#ifndef KCMRULES

//************************************
// StrutRect
//************************************

StrutRect::StrutRect(QRect rect, StrutArea area)
    : QRect(rect)
    , m_area(area)
{
}

StrutRect::StrutRect(int x, int y, int width, int height, StrutArea area)
    : QRect(x, y, width, height)
    , m_area(area)
{
}

StrutRect::StrutRect(const StrutRect& other)
    : QRect(other)
    , m_area(other.area())
{
}

StrutRect &StrutRect::operator=(const StrutRect &other)
{
    if (this != &other) {
        QRect::operator=(other);
        m_area = other.area();
    }
    return *this;
}

#endif

Process::Process(QObject *parent)
    : QProcess(parent)
{
}

Process::~Process() = default;

#ifndef KCMRULES
void updateXTime()
{
    kwinApp()->platform()->updateXTime();
}

static int server_grab_count = 0;

void grabXServer()
{
    if (++server_grab_count == 1)
        xcb_grab_server(connection());
}

void ungrabXServer()
{
    Q_ASSERT(server_grab_count > 0);
    if (--server_grab_count == 0) {
        xcb_ungrab_server(connection());
        xcb_flush(connection());
    }
}

static bool keyboard_grabbed = false;

bool grabXKeyboard(xcb_window_t w)
{
    if (QWidget::keyboardGrabber() != nullptr)
        return false;
    if (keyboard_grabbed)
        return false;
    if (qApp->activePopupWidget() != nullptr)
        return false;
    if (w == XCB_WINDOW_NONE)
        w = rootWindow();
    const xcb_grab_keyboard_cookie_t c = xcb_grab_keyboard_unchecked(connection(), false, w, xTime(),
                                                                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    ScopedCPointer<xcb_grab_keyboard_reply_t> grab(xcb_grab_keyboard_reply(connection(), c, nullptr));
    if (grab.isNull()) {
        return false;
    }
    if (grab->status != XCB_GRAB_STATUS_SUCCESS) {
        return false;
    }
    keyboard_grabbed = true;
    return true;
}

void ungrabXKeyboard()
{
    if (!keyboard_grabbed) {
        // grabXKeyboard() may fail sometimes, so don't fail, but at least warn anyway
        qCDebug(KWIN_CORE) << "ungrabXKeyboard() called but keyboard not grabbed!";
    }
    keyboard_grabbed = false;
    xcb_ungrab_keyboard(connection(), XCB_TIME_CURRENT_TIME);
}

void Process::setupChildProcess()
{
    sigset_t userSignals;
    sigemptyset(&userSignals);
    sigaddset(&userSignals, SIGUSR1);
    sigaddset(&userSignals, SIGUSR2);
    pthread_sigmask(SIG_UNBLOCK, &userSignals, nullptr);
}

#endif

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states

Qt::MouseButton x11ToQtMouseButton(int button)
{
    if (button == XCB_BUTTON_INDEX_1)
        return Qt::LeftButton;
    if (button == XCB_BUTTON_INDEX_2)
        return Qt::MiddleButton;
    if (button == XCB_BUTTON_INDEX_3)
        return Qt::RightButton;
    if (button == XCB_BUTTON_INDEX_4)
        return Qt::XButton1;
    if (button == XCB_BUTTON_INDEX_5)
        return Qt::XButton2;
    return Qt::NoButton;
}

Qt::MouseButtons x11ToQtMouseButtons(int state)
{
    Qt::MouseButtons ret = {};
    if (state & XCB_KEY_BUT_MASK_BUTTON_1)
        ret |= Qt::LeftButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_2)
        ret |= Qt::MiddleButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_3)
        ret |= Qt::RightButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_4)
        ret |= Qt::XButton1;
    if (state & XCB_KEY_BUT_MASK_BUTTON_5)
        ret |= Qt::XButton2;
    return ret;
}

Qt::KeyboardModifiers x11ToQtKeyboardModifiers(int state)
{
    Qt::KeyboardModifiers ret = {};
    if (state & XCB_KEY_BUT_MASK_SHIFT)
        ret |= Qt::ShiftModifier;
    if (state & XCB_KEY_BUT_MASK_CONTROL)
        ret |= Qt::ControlModifier;
    if (state & KKeyServer::modXAlt())
        ret |= Qt::AltModifier;
    if (state & KKeyServer::modXMeta())
        ret |= Qt::MetaModifier;
    return ret;
}

} // namespace

#ifndef KCMRULES
#endif

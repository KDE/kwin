/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "connection.h"
#include "context.h"
#include "events.h"
#include "../logind.h"
#include "../udev.h"
#include "libinput_logging.h"

#include <QMutexLocker>
#include <QSocketNotifier>
#include <QThread>

#include <libinput.h>

namespace KWin
{
namespace LibInput
{

Connection *Connection::s_self = nullptr;
QThread *Connection::s_thread = nullptr;

static Context *s_context = nullptr;

Connection::Connection(QObject *parent)
    : Connection(nullptr, parent)
{
    // only here to fix build, using will crash, BUG 343529
}

Connection *Connection::create(QObject *parent)
{
    Q_ASSERT(!s_self);
    static Udev s_udev;
    if (!s_udev.isValid()) {
        qCWarning(KWIN_LIBINPUT) << "Failed to initialize udev";
        return nullptr;
    }
    if (!s_context) {
        s_context = new Context(s_udev);
        if (!s_context->isValid()) {
            qCWarning(KWIN_LIBINPUT) << "Failed to create context from udev";
            delete s_context;
            s_context = nullptr;
            return nullptr;
        }
        // TODO: don't hardcode seat name
        if (!s_context->assignSeat("seat0")) {
            qCWarning(KWIN_LIBINPUT) << "Failed to assign seat seat0";
            delete s_context;
            s_context = nullptr;
            return nullptr;
        }
    }
    s_thread = new QThread();
    s_self = new Connection(s_context);
    s_self->moveToThread(s_thread);
    s_thread->start();
    QObject::connect(s_thread, &QThread::finished, s_self, &QObject::deleteLater);
    QObject::connect(s_thread, &QThread::finished, s_thread, &QObject::deleteLater);
    QObject::connect(parent, &QObject::destroyed, s_thread, &QThread::quit);
    return s_self;
}

Connection::Connection(Context *input, QObject *parent)
    : QObject(parent)
    , m_input(input)
    , m_notifier(nullptr)
    , m_mutex(QMutex::Recursive)
{
    Q_ASSERT(m_input);
}

Connection::~Connection()
{
    s_self = nullptr;
    delete s_context;
    s_context = nullptr;
}

void Connection::setup()
{
    QMetaObject::invokeMethod(this, "doSetup", Qt::QueuedConnection);
}

void Connection::doSetup()
{
    Q_ASSERT(!m_notifier);
    m_notifier = new QSocketNotifier(m_input->fileDescriptor(), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Connection::handleEvent);

    LogindIntegration *logind = LogindIntegration::self();
    connect(logind, &LogindIntegration::sessionActiveChanged, this,
        [this](bool active) {
            if (active) {
                if (!m_input->isSuspended()) {
                    return;
                }
                m_input->resume();
                wasSuspended = true;
            } else {
                deactivate();
            }
        }
    );
    handleEvent();
}

void Connection::deactivate()
{
    if (m_input->isSuspended()) {
        return;
    }
    m_keyboardBeforeSuspend = hasKeyboard();
    m_pointerBeforeSuspend = hasPointer();
    m_touchBeforeSuspend = hasTouch();
    m_input->suspend();
    handleEvent();
}

void Connection::handleEvent()
{
    QMutexLocker locker(&m_mutex);
    const bool wasEmpty = m_eventQueue.isEmpty();
    do {
        m_input->dispatch();
        Event *event = m_input->event();
        if (!event) {
            break;
        }
        m_eventQueue << event;
    } while (true);
    if (wasEmpty && !m_eventQueue.isEmpty()) {
        emit eventsRead();
    }
}

void Connection::processEvents()
{
    QMutexLocker locker(&m_mutex);
    while (!m_eventQueue.isEmpty()) {
        QScopedPointer<Event> event(m_eventQueue.takeFirst());
        switch (event->type()) {
            case LIBINPUT_EVENT_DEVICE_ADDED:
                if (libinput_device_has_capability(event->device(), LIBINPUT_DEVICE_CAP_KEYBOARD)) {
                    m_keyboard++;
                    if (m_keyboard == 1) {
                        emit hasKeyboardChanged(true);
                    }
                }
                if (libinput_device_has_capability(event->device(), LIBINPUT_DEVICE_CAP_POINTER)) {
                    m_pointer++;
                    if (m_pointer == 1) {
                        emit hasPointerChanged(true);
                    }
                }
                if (libinput_device_has_capability(event->device(), LIBINPUT_DEVICE_CAP_TOUCH)) {
                    m_touch++;
                    if (m_touch == 1) {
                        emit hasTouchChanged(true);
                    }
                }
                break;
            case LIBINPUT_EVENT_DEVICE_REMOVED:
                if (libinput_device_has_capability(event->device(), LIBINPUT_DEVICE_CAP_KEYBOARD)) {
                    m_keyboard--;
                    if (m_keyboard == 0) {
                        emit hasKeyboardChanged(false);
                    }
                }
                if (libinput_device_has_capability(event->device(), LIBINPUT_DEVICE_CAP_POINTER)) {
                    m_pointer--;
                    if (m_pointer == 0) {
                        emit hasPointerChanged(false);
                    }
                }
                if (libinput_device_has_capability(event->device(), LIBINPUT_DEVICE_CAP_TOUCH)) {
                    m_touch--;
                    if (m_touch == 0) {
                        emit hasTouchChanged(false);
                    }
                }
                break;
            case LIBINPUT_EVENT_KEYBOARD_KEY: {
                KeyEvent *ke = static_cast<KeyEvent*>(event.data());
                emit keyChanged(ke->key(), ke->state(), ke->time());
                break;
            }
            case LIBINPUT_EVENT_POINTER_AXIS: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                const auto axis = pe->axis();
                for (auto it = axis.begin(); it != axis.end(); ++it) {
                    emit pointerAxisChanged(*it, pe->axisValue(*it), pe->time());
                }
                break;
            }
            case LIBINPUT_EVENT_POINTER_BUTTON: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                emit pointerButtonChanged(pe->button(), pe->buttonState(), pe->time());
                break;
            }
            case LIBINPUT_EVENT_POINTER_MOTION: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                emit pointerMotion(pe->delta(), pe->time());
                break;
            }
            case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                emit pointerMotionAbsolute(pe->absolutePos(), pe->absolutePos(m_size), pe->time());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_DOWN: {
                TouchEvent *te = static_cast<TouchEvent*>(event.data());
                emit touchDown(te->id(), te->absolutePos(m_size), te->time());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_UP: {
                TouchEvent *te = static_cast<TouchEvent*>(event.data());
                emit touchUp(te->id(), te->time());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_MOTION: {
                TouchEvent *te = static_cast<TouchEvent*>(event.data());
                emit touchMotion(te->id(), te->absolutePos(m_size), te->time());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_CANCEL: {
                emit touchCanceled();
                break;
            }
            case LIBINPUT_EVENT_TOUCH_FRAME: {
                emit touchFrame();
                break;
            }
            default:
                // nothing
                break;
        }
    }
    if (wasSuspended) {
        if (m_keyboardBeforeSuspend && !m_keyboard) {
            emit hasKeyboardChanged(false);
        }
        if (m_pointerBeforeSuspend && !m_pointer) {
            emit hasPointerChanged(false);
        }
        if (m_touchBeforeSuspend && !m_touch) {
            emit hasTouchChanged(false);
        }
        wasSuspended = false;
    }
}

void Connection::setScreenSize(const QSize &size)
{
    m_size = size;
}

bool Connection::isSuspended() const
{
    if (!s_context) {
        return false;
    }
    return s_context->isSuspended();
}

}
}

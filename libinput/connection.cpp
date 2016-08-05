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
#include "device.h"
#include "events.h"
#include "../logind.h"
#include "../udev.h"
#include "libinput_logging.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingCall>
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

static const QString s_touchpadComponent = QStringLiteral("kcm_touchpad");

Connection::Connection(Context *input, QObject *parent)
    : QObject(parent)
    , m_input(input)
    , m_notifier(nullptr)
    , m_mutex(QMutex::Recursive)
{
    Q_ASSERT(m_input);

    // steal touchpad shortcuts
    QAction *touchpadToggleAction = new QAction(this);
    QAction *touchpadOnAction = new QAction(this);
    QAction *touchpadOffAction = new QAction(this);

    touchpadToggleAction->setObjectName(QStringLiteral("Toggle Touchpad"));
    touchpadToggleAction->setProperty("componentName", s_touchpadComponent);
    touchpadOnAction->setObjectName(QStringLiteral("Enable Touchpad"));
    touchpadOnAction->setProperty("componentName", s_touchpadComponent);
    touchpadOffAction->setObjectName(QStringLiteral("Disable Touchpad"));
    touchpadOffAction->setProperty("componentName", s_touchpadComponent);
    KGlobalAccel::self()->setDefaultShortcut(touchpadToggleAction, QList<QKeySequence>{Qt::Key_TouchpadToggle});
    KGlobalAccel::self()->setShortcut(touchpadToggleAction, QList<QKeySequence>{Qt::Key_TouchpadToggle});
    KGlobalAccel::self()->setDefaultShortcut(touchpadOnAction, QList<QKeySequence>{Qt::Key_TouchpadOn});
    KGlobalAccel::self()->setShortcut(touchpadOnAction, QList<QKeySequence>{Qt::Key_TouchpadOn});
    KGlobalAccel::self()->setDefaultShortcut(touchpadOffAction, QList<QKeySequence>{Qt::Key_TouchpadOff});
    KGlobalAccel::self()->setShortcut(touchpadOffAction, QList<QKeySequence>{Qt::Key_TouchpadOff});
#ifndef KWIN_BUILD_TESTING
    InputRedirection::self()->registerShortcut(Qt::Key_TouchpadToggle, touchpadToggleAction);
    InputRedirection::self()->registerShortcut(Qt::Key_TouchpadOn, touchpadOnAction);
    InputRedirection::self()->registerShortcut(Qt::Key_TouchpadOff, touchpadOffAction);
#endif
    connect(touchpadToggleAction, &QAction::triggered, this, &Connection::toggleTouchpads);
    connect(touchpadOnAction, &QAction::triggered, this,
        [this] {
            if (m_touchpadsEnabled) {
                return;
            }
            toggleTouchpads();
        }
    );
    connect(touchpadOffAction, &QAction::triggered, this,
        [this] {
            if (!m_touchpadsEnabled) {
                return;
            }
            toggleTouchpads();
        }
    );

    // need to connect to KGlobalSettings as the mouse KCM does not emit a dedicated signal
    QDBusConnection::sessionBus().connect(QString(), QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"),
                                          QStringLiteral("notifyChange"), this, SLOT(slotKGlobalSettingsNotifyChange(int,int)));
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
    m_alphaNumericKeyboardBeforeSuspend = hasAlphaNumericKeyboard();
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
            case LIBINPUT_EVENT_DEVICE_ADDED: {
                auto device = new Device(event->nativeDevice());
                device->moveToThread(s_thread);
                device->setParent(this);
                m_devices << device;
                if (device->isKeyboard()) {
                    m_keyboard++;
                    if (device->isAlphaNumericKeyboard()) {
                        m_alphaNumericKeyboard++;
                        if (m_alphaNumericKeyboard == 1) {
                            emit hasAlphaNumericKeyboardChanged(true);
                        }
                    }
                    if (m_keyboard == 1) {
                        emit hasKeyboardChanged(true);
                    }
                }
                if (device->isPointer()) {
                    m_pointer++;
                    if (m_pointer == 1) {
                        emit hasPointerChanged(true);
                    }
                }
                if (device->isTouch()) {
                    m_touch++;
                    if (m_touch == 1) {
                        emit hasTouchChanged(true);
                    }
                }
                applyDeviceConfig(device);
                emit deviceAdded(device);
                break;
            }
            case LIBINPUT_EVENT_DEVICE_REMOVED: {
                auto it = std::find_if(m_devices.begin(), m_devices.end(), [&event] (Device *d) { return event->device() == d; } );
                if (it == m_devices.end()) {
                    // we don't know this device
                    break;
                }
                auto device = *it;
                m_devices.erase(it);
                emit deviceRemoved(device);

                if (device->isKeyboard()) {
                    m_keyboard--;
                    if (device->isAlphaNumericKeyboard()) {
                        m_alphaNumericKeyboard--;
                        if (m_alphaNumericKeyboard == 0) {
                            emit hasAlphaNumericKeyboardChanged(false);
                        }
                    }
                    if (m_keyboard == 0) {
                        emit hasKeyboardChanged(false);
                    }
                }
                if (device->isPointer()) {
                    m_pointer--;
                    if (m_pointer == 0) {
                        emit hasPointerChanged(false);
                    }
                }
                if (device->isTouch()) {
                    m_touch--;
                    if (m_touch == 0) {
                        emit hasTouchChanged(false);
                    }
                }
                device->deleteLater();
                break;
            }
            case LIBINPUT_EVENT_KEYBOARD_KEY: {
                KeyEvent *ke = static_cast<KeyEvent*>(event.data());
                emit keyChanged(ke->key(), ke->state(), ke->time(), ke->device());
                break;
            }
            case LIBINPUT_EVENT_POINTER_AXIS: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                struct Axis {
                    qreal delta = 0.0;
                    quint32 time = 0;
                };
                QMap<InputRedirection::PointerAxis, Axis> deltas;
                auto update = [&deltas] (PointerEvent *pe) {
                    const auto axis = pe->axis();
                    for (auto it = axis.begin(); it != axis.end(); ++it) {
                        deltas[*it].delta += pe->axisValue(*it);
                        deltas[*it].time = pe->time();
                    }
                };
                update(pe);
                auto it = m_eventQueue.begin();
                while (it != m_eventQueue.end()) {
                    if ((*it)->type() == LIBINPUT_EVENT_POINTER_AXIS) {
                        QScopedPointer<PointerEvent> p(static_cast<PointerEvent*>(*it));
                        update(p.data());
                        it = m_eventQueue.erase(it);
                    } else {
                        break;
                    }
                }
                for (auto it = deltas.constBegin(); it != deltas.constEnd(); ++it) {
                    emit pointerAxisChanged(it.key(), it.value().delta, it.value().time, pe->device());
                }
                break;
            }
            case LIBINPUT_EVENT_POINTER_BUTTON: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                emit pointerButtonChanged(pe->button(), pe->buttonState(), pe->time(), pe->device());
                break;
            }
            case LIBINPUT_EVENT_POINTER_MOTION: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                QPointF delta = pe->delta();
                quint32 latestTime = pe->time();
                auto it = m_eventQueue.begin();
                while (it != m_eventQueue.end()) {
                    if ((*it)->type() == LIBINPUT_EVENT_POINTER_MOTION) {
                        QScopedPointer<PointerEvent> p(static_cast<PointerEvent*>(*it));
                        delta += p->delta();
                        latestTime = p->time();
                        it = m_eventQueue.erase(it);
                    } else {
                        break;
                    }
                }
                emit pointerMotion(delta, latestTime, pe->device());
                break;
            }
            case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
                PointerEvent *pe = static_cast<PointerEvent*>(event.data());
                emit pointerMotionAbsolute(pe->absolutePos(), pe->absolutePos(m_size), pe->time(), pe->device());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_DOWN: {
                TouchEvent *te = static_cast<TouchEvent*>(event.data());
                emit touchDown(te->id(), te->absolutePos(m_size), te->time(), te->device());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_UP: {
                TouchEvent *te = static_cast<TouchEvent*>(event.data());
                emit touchUp(te->id(), te->time(), te->device());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_MOTION: {
                TouchEvent *te = static_cast<TouchEvent*>(event.data());
                emit touchMotion(te->id(), te->absolutePos(m_size), te->time(), te->device());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_CANCEL: {
                emit touchCanceled(event->device());
                break;
            }
            case LIBINPUT_EVENT_TOUCH_FRAME: {
                emit touchFrame(event->device());
                break;
            }
            case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN: {
                PinchGestureEvent *pe = static_cast<PinchGestureEvent*>(event.data());
                emit pinchGestureBegin(pe->fingerCount(), pe->time(), pe->device());
                break;
            }
            case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE: {
                PinchGestureEvent *pe = static_cast<PinchGestureEvent*>(event.data());
                emit pinchGestureUpdate(pe->scale(), pe->angleDelta(), pe->delta(), pe->time(), pe->device());
                break;
            }
            case LIBINPUT_EVENT_GESTURE_PINCH_END: {
                PinchGestureEvent *pe = static_cast<PinchGestureEvent*>(event.data());
                if (pe->isCancelled()) {
                    emit pinchGestureCancelled(pe->time(), pe->device());
                } else {
                    emit pinchGestureEnd(pe->time(), pe->device());
                }
                break;
            }
            case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN: {
                SwipeGestureEvent *se = static_cast<SwipeGestureEvent*>(event.data());
                emit swipeGestureBegin(se->fingerCount(), se->time(), se->device());
                break;
            }
            case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE: {
                SwipeGestureEvent *se = static_cast<SwipeGestureEvent*>(event.data());
                emit swipeGestureUpdate(se->delta(), se->time(), se->device());
                break;
            }
            case LIBINPUT_EVENT_GESTURE_SWIPE_END: {
                SwipeGestureEvent *se = static_cast<SwipeGestureEvent*>(event.data());
                if (se->isCancelled()) {
                    emit swipeGestureCancelled(se->time(), se->device());
                } else {
                    emit swipeGestureEnd(se->time(), se->device());
                }
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
        if (m_alphaNumericKeyboardBeforeSuspend && !m_alphaNumericKeyboard) {
            emit hasAlphaNumericKeyboardChanged(false);
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

void Connection::applyDeviceConfig(Device *device)
{
    if (device->isPointer()) {
        const KConfigGroup group = m_config->group("Mouse");
        device->setLeftHanded(group.readEntry("MouseButtonMapping", "RightHanded") == QLatin1String("LeftHanded"));
        qreal accel = group.readEntry("Acceleration", -1.0);
        if (qFuzzyCompare(accel, -1.0) || qFuzzyCompare(accel, 1.0)) {
            // default value
            device->setPointerAcceleration(0.0);
        } else {
            // the X11-based config is mapped in [0.1,20.0] with 1.0 being the "normal" setting - we assume that's the default
            if (accel < 1.0) {
                device->setPointerAcceleration(-1.0 + ((accel * 10.0) - 1.0) / 9.0);
            } else {
                device->setPointerAcceleration((accel -1.0)/19.0);
            }
        }
    }
}

void Connection::slotKGlobalSettingsNotifyChange(int type, int arg)
{
    if (type == 3 /**SettingsChanged**/ && arg == 0 /** SETTINGS_MOUSE **/) {
        m_config->reparseConfiguration();
        for (auto it = m_devices.constBegin(), end = m_devices.constEnd(); it != end; ++it) {
            if ((*it)->isPointer()) {
                applyDeviceConfig(*it);
            }
        }
    }
}

void Connection::toggleTouchpads()
{
    bool changed = false;
    m_touchpadsEnabled = !m_touchpadsEnabled;
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        auto device = *it;
        if (!device->isPointer()) {
            continue;
        }
        if (device->isKeyboard() || device->isTouch() || device->isTabletPad() || device->isTabletTool()) {
            // ignore all combined devices. E.g. a touchpad on a keyboard we don't want to toggle
            // as that would result in the keyboard going off as well
            continue;
        }
        // is this a touch pad? We don't really know, let's do some assumptions
        if (device->tapFingerCount() > 0 || device->supportsDisableWhileTyping() || device->supportsDisableEventsOnExternalMouse()) {
            const bool old = device->isEnabled();
            device->setEnabled(m_touchpadsEnabled);
            if (old != device->isEnabled()) {
                changed = true;
            }
        }
    }
    if (changed) {
        // send OSD message
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.plasmashell"),
            QStringLiteral("/org/kde/osdService"),
            QStringLiteral("org.kde.osdService"),
            QStringLiteral("touchpadEnabledChanged")
        );
        msg.setArguments({m_touchpadsEnabled});
        QDBusConnection::sessionBus().asyncCall(msg);
    }
}

}
}

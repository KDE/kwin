/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "egl_hwcomposer_backend.h"
#include "hwcomposer_backend.h"
#include "logging.h"
#include "screens_hwcomposer.h"
#include "composite.h"
#include "wayland_server.h"
// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/seat_interface.h>
// hybris/android
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hybris/input/input_stack_compatibility_layer.h>
#include <hybris/input/input_stack_compatibility_layer_codes_key.h>
#include <hybris/input/input_stack_compatibility_layer_flags_key.h>
#include <hybris/input/input_stack_compatibility_layer_flags_motion.h>
// linux
#include <linux/input.h>

// based on test_hwcomposer.c from libhybris project (Apache 2 licensed)

namespace KWin
{

HwcomposerBackend::HwcomposerBackend(QObject *parent)
    : AbstractBackend(parent)
{
    handleOutputs();
}

HwcomposerBackend::~HwcomposerBackend()
{
    if (m_device) {
        hwc_close_1(m_device);
    }
    if (m_inputListener) {
        android_input_stack_stop();
        android_input_stack_shutdown();
        delete m_inputListener;
    }
}

static uint eventTouchId(Event *event, uint index)
{
    Q_ASSERT(index < event->details.motion.pointer_count);
    return event->details.motion.pointer_coordinates[index].id;
}

static QPointF eventTouchPosition(Event *event, uint index)
{
    Q_ASSERT(index < event->details.motion.pointer_count);
    return QPointF(event->details.motion.pointer_coordinates[index].x,
                   event->details.motion.pointer_coordinates[index].y);
}

static qint32 translateKey(qint32 key)
{
    static const QHash<qint32, qint32> s_translation = {
        {ISCL_KEYCODE_UNKNOWN,            KEY_RESERVED},
        {ISCL_KEYCODE_SOFT_LEFT,          KEY_RESERVED},
        {ISCL_KEYCODE_SOFT_RIGHT,         KEY_RESERVED},
        {ISCL_KEYCODE_HOME,               KEY_HOME},
        {ISCL_KEYCODE_BACK,               KEY_BACK},
        {ISCL_KEYCODE_CALL,               KEY_RESERVED},
        {ISCL_KEYCODE_ENDCALL,            KEY_RESERVED},
        {ISCL_KEYCODE_0,                  KEY_0},
        {ISCL_KEYCODE_1,                  KEY_1},
        {ISCL_KEYCODE_2,                  KEY_2},
        {ISCL_KEYCODE_3,                  KEY_3},
        {ISCL_KEYCODE_4,                  KEY_4},
        {ISCL_KEYCODE_5,                  KEY_5},
        {ISCL_KEYCODE_6,                  KEY_6},
        {ISCL_KEYCODE_7,                  KEY_7},
        {ISCL_KEYCODE_8,                  KEY_8},
        {ISCL_KEYCODE_9,                  KEY_9},
        {ISCL_KEYCODE_STAR,               KEY_NUMERIC_STAR},
        {ISCL_KEYCODE_POUND,              KEY_NUMERIC_POUND},
        {ISCL_KEYCODE_DPAD_UP,            BTN_DPAD_UP},
        {ISCL_KEYCODE_DPAD_DOWN,          BTN_DPAD_DOWN},
        {ISCL_KEYCODE_DPAD_LEFT,          BTN_DPAD_LEFT},
        {ISCL_KEYCODE_DPAD_RIGHT,         BTN_DPAD_RIGHT},
        {ISCL_KEYCODE_DPAD_CENTER,        KEY_RESERVED},
        {ISCL_KEYCODE_VOLUME_UP,          KEY_VOLUMEUP},
        {ISCL_KEYCODE_VOLUME_DOWN,        KEY_VOLUMEDOWN},
        {ISCL_KEYCODE_POWER,              KEY_POWER},
        {ISCL_KEYCODE_CAMERA,             KEY_CAMERA},
        {ISCL_KEYCODE_CLEAR,              KEY_CLEAR},
        {ISCL_KEYCODE_A,                  KEY_A},
        {ISCL_KEYCODE_B,                  KEY_B},
        {ISCL_KEYCODE_C,                  KEY_C},
        {ISCL_KEYCODE_D,                  KEY_D},
        {ISCL_KEYCODE_E,                  KEY_E},
        {ISCL_KEYCODE_F,                  KEY_F},
        {ISCL_KEYCODE_G,                  KEY_G},
        {ISCL_KEYCODE_H,                  KEY_H},
        {ISCL_KEYCODE_I,                  KEY_I},
        {ISCL_KEYCODE_J,                  KEY_J},
        {ISCL_KEYCODE_K,                  KEY_K},
        {ISCL_KEYCODE_L,                  KEY_L},
        {ISCL_KEYCODE_M,                  KEY_M},
        {ISCL_KEYCODE_N,                  KEY_N},
        {ISCL_KEYCODE_O,                  KEY_O},
        {ISCL_KEYCODE_P,                  KEY_P},
        {ISCL_KEYCODE_Q,                  KEY_Q},
        {ISCL_KEYCODE_R,                  KEY_R},
        {ISCL_KEYCODE_S,                  KEY_S},
        {ISCL_KEYCODE_T,                  KEY_T},
        {ISCL_KEYCODE_U,                  KEY_U},
        {ISCL_KEYCODE_V,                  KEY_V},
        {ISCL_KEYCODE_W,                  KEY_W},
        {ISCL_KEYCODE_X,                  KEY_X},
        {ISCL_KEYCODE_Y,                  KEY_Y},
        {ISCL_KEYCODE_Z,                  KEY_Z},
        {ISCL_KEYCODE_COMMA,              KEY_COMMA},
        {ISCL_KEYCODE_PERIOD,             KEY_DOT},
        {ISCL_KEYCODE_ALT_LEFT,           KEY_LEFTALT},
        {ISCL_KEYCODE_ALT_RIGHT,          KEY_RIGHTALT},
        {ISCL_KEYCODE_SHIFT_LEFT,         KEY_LEFTSHIFT},
        {ISCL_KEYCODE_SHIFT_RIGHT,        KEY_RIGHTSHIFT},
        {ISCL_KEYCODE_TAB,                KEY_TAB},
        {ISCL_KEYCODE_SPACE,              KEY_SPACE},
        {ISCL_KEYCODE_SYM,                KEY_RESERVED},
        {ISCL_KEYCODE_EXPLORER,           KEY_RESERVED},
        {ISCL_KEYCODE_ENVELOPE,           KEY_EMAIL},
        {ISCL_KEYCODE_ENTER,              KEY_ENTER},
        {ISCL_KEYCODE_DEL,                KEY_DELETE},
        {ISCL_KEYCODE_GRAVE,              KEY_GRAVE},
        {ISCL_KEYCODE_MINUS,              KEY_MINUS},
        {ISCL_KEYCODE_EQUALS,             KEY_EQUAL},
        {ISCL_KEYCODE_LEFT_BRACKET,       KEY_LEFTBRACE},
        {ISCL_KEYCODE_RIGHT_BRACKET,      KEY_RIGHTBRACE},
        {ISCL_KEYCODE_BACKSLASH,          KEY_BACKSLASH},
        {ISCL_KEYCODE_SEMICOLON,          KEY_SEMICOLON},
        {ISCL_KEYCODE_APOSTROPHE,         KEY_APOSTROPHE},
        {ISCL_KEYCODE_SLASH,              KEY_SLASH},
        {ISCL_KEYCODE_AT,                 KEY_RESERVED},
        {ISCL_KEYCODE_NUM,                KEY_RESERVED},
        {ISCL_KEYCODE_HEADSETHOOK,        KEY_RESERVED},
        {ISCL_KEYCODE_FOCUS,              KEY_CAMERA_FOCUS},
        {ISCL_KEYCODE_PLUS,               KEY_RESERVED},
        {ISCL_KEYCODE_MENU,               KEY_MENU},
        {ISCL_KEYCODE_NOTIFICATION,       KEY_RESERVED},
        {ISCL_KEYCODE_SEARCH,             KEY_SEARCH},
        {ISCL_KEYCODE_MEDIA_PLAY_PAUSE,   KEY_PLAYPAUSE},
        {ISCL_KEYCODE_MEDIA_STOP,         KEY_STOPCD},
        {ISCL_KEYCODE_MEDIA_NEXT,         KEY_NEXTSONG},
        {ISCL_KEYCODE_MEDIA_PREVIOUS,     KEY_PREVIOUSSONG},
        {ISCL_KEYCODE_MEDIA_REWIND,       KEY_REWIND},
        {ISCL_KEYCODE_MEDIA_FAST_FORWARD, KEY_FASTFORWARD},
        {ISCL_KEYCODE_MUTE,               KEY_MUTE},
        {ISCL_KEYCODE_PAGE_UP,            KEY_PAGEUP},
        {ISCL_KEYCODE_PAGE_DOWN,          KEY_PAGEDOWN},
        {ISCL_KEYCODE_PICTSYMBOLS,        KEY_RESERVED},
        {ISCL_KEYCODE_SWITCH_CHARSET,     KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_A,           BTN_A},
        {ISCL_KEYCODE_BUTTON_B,           BTN_B},
        {ISCL_KEYCODE_BUTTON_C,           BTN_C},
        {ISCL_KEYCODE_BUTTON_X,           BTN_X},
        {ISCL_KEYCODE_BUTTON_Y,           BTN_Y},
        {ISCL_KEYCODE_BUTTON_Z,           BTN_Z},
        {ISCL_KEYCODE_BUTTON_L1,          BTN_TL},
        {ISCL_KEYCODE_BUTTON_R1,          BTN_TR},
        {ISCL_KEYCODE_BUTTON_L2,          BTN_TL2},
        {ISCL_KEYCODE_BUTTON_R2,          BTN_TR2},
        {ISCL_KEYCODE_BUTTON_THUMBL,      BTN_THUMBL},
        {ISCL_KEYCODE_BUTTON_THUMBR,      BTN_THUMBR},
        {ISCL_KEYCODE_BUTTON_START,       BTN_START},
        {ISCL_KEYCODE_BUTTON_SELECT,      BTN_SELECT},
        {ISCL_KEYCODE_BUTTON_MODE,        BTN_MODE},
        {ISCL_KEYCODE_ESCAPE,             KEY_ESC},
        {ISCL_KEYCODE_FORWARD_DEL,        KEY_RESERVED},
        {ISCL_KEYCODE_CTRL_LEFT,          KEY_LEFTCTRL},
        {ISCL_KEYCODE_CTRL_RIGHT,         KEY_RIGHTCTRL},
        {ISCL_KEYCODE_CAPS_LOCK,          KEY_CAPSLOCK},
        {ISCL_KEYCODE_SCROLL_LOCK,        KEY_SCROLLLOCK},
        {ISCL_KEYCODE_META_LEFT,          KEY_LEFTMETA},
        {ISCL_KEYCODE_META_RIGHT,         KEY_RIGHTMETA},
        {ISCL_KEYCODE_FUNCTION,           KEY_RESERVED},
        {ISCL_KEYCODE_SYSRQ,              KEY_SYSRQ},
        {ISCL_KEYCODE_BREAK,              KEY_RESERVED},
        {ISCL_KEYCODE_MOVE_HOME,          KEY_HOME},
        {ISCL_KEYCODE_MOVE_END,           KEY_END},
        {ISCL_KEYCODE_INSERT,             KEY_INSERT},
        {ISCL_KEYCODE_FORWARD,            KEY_RESERVED},
        {ISCL_KEYCODE_MEDIA_PLAY,         KEY_PLAYCD},
        {ISCL_KEYCODE_MEDIA_PAUSE,        KEY_PAUSECD},
        {ISCL_KEYCODE_MEDIA_CLOSE,        KEY_CLOSECD},
        {ISCL_KEYCODE_MEDIA_EJECT,        KEY_EJECTCD},
        {ISCL_KEYCODE_MEDIA_RECORD,       KEY_RECORD},
        {ISCL_KEYCODE_F1,                 KEY_F1},
        {ISCL_KEYCODE_F2,                 KEY_F2},
        {ISCL_KEYCODE_F3,                 KEY_F3},
        {ISCL_KEYCODE_F4,                 KEY_F4},
        {ISCL_KEYCODE_F5,                 KEY_F5},
        {ISCL_KEYCODE_F6,                 KEY_F6},
        {ISCL_KEYCODE_F7,                 KEY_F7},
        {ISCL_KEYCODE_F8,                 KEY_F8},
        {ISCL_KEYCODE_F9,                 KEY_F9},
        {ISCL_KEYCODE_F10,                KEY_F10},
        {ISCL_KEYCODE_F11,                KEY_F11},
        {ISCL_KEYCODE_F12,                KEY_F12},
        {ISCL_KEYCODE_NUM_LOCK,           KEY_NUMLOCK},
        {ISCL_KEYCODE_NUMPAD_0,           KEY_KP0},
        {ISCL_KEYCODE_NUMPAD_1,           KEY_KP1},
        {ISCL_KEYCODE_NUMPAD_2,           KEY_KP2},
        {ISCL_KEYCODE_NUMPAD_3,           KEY_KP3},
        {ISCL_KEYCODE_NUMPAD_4,           KEY_KP4},
        {ISCL_KEYCODE_NUMPAD_5,           KEY_KP5},
        {ISCL_KEYCODE_NUMPAD_6,           KEY_KP6},
        {ISCL_KEYCODE_NUMPAD_7,           KEY_KP7},
        {ISCL_KEYCODE_NUMPAD_8,           KEY_KP8},
        {ISCL_KEYCODE_NUMPAD_9,           KEY_KP9},
        {ISCL_KEYCODE_NUMPAD_DIVIDE,      KEY_KPSLASH},
        {ISCL_KEYCODE_NUMPAD_MULTIPLY,    KEY_KPASTERISK},
        {ISCL_KEYCODE_NUMPAD_SUBTRACT,    KEY_KPMINUS},
        {ISCL_KEYCODE_NUMPAD_ADD,         KEY_KPPLUS},
        {ISCL_KEYCODE_NUMPAD_DOT,         KEY_KPDOT},
        {ISCL_KEYCODE_NUMPAD_COMMA,       KEY_KPCOMMA},
        {ISCL_KEYCODE_NUMPAD_ENTER,       KEY_KPENTER},
        {ISCL_KEYCODE_NUMPAD_EQUALS,      KEY_KPEQUAL},
        {ISCL_KEYCODE_NUMPAD_LEFT_PAREN,  KEY_KPLEFTPAREN},
        {ISCL_KEYCODE_NUMPAD_RIGHT_PAREN, KEY_KPRIGHTPAREN},
        {ISCL_KEYCODE_VOLUME_MUTE,        KEY_MUTE},
        {ISCL_KEYCODE_INFO,               KEY_RESERVED},
        {ISCL_KEYCODE_CHANNEL_UP,         KEY_CHANNELUP},
        {ISCL_KEYCODE_CHANNEL_DOWN,       KEY_CHANNELDOWN},
        {ISCL_KEYCODE_ZOOM_IN,            KEY_ZOOMIN},
        {ISCL_KEYCODE_ZOOM_OUT,           KEY_ZOOMOUT},
        {ISCL_KEYCODE_TV,                 KEY_RESERVED},
        {ISCL_KEYCODE_WINDOW,             KEY_RESERVED},
        {ISCL_KEYCODE_GUIDE,              KEY_RESERVED},
        {ISCL_KEYCODE_DVR,                KEY_RESERVED},
        {ISCL_KEYCODE_BOOKMARK,           KEY_RESERVED},
        {ISCL_KEYCODE_CAPTIONS,           KEY_RESERVED},
        {ISCL_KEYCODE_SETTINGS,           KEY_RESERVED},
        {ISCL_KEYCODE_TV_POWER,           KEY_RESERVED},
        {ISCL_KEYCODE_TV_INPUT,           KEY_RESERVED},
        {ISCL_KEYCODE_STB_POWER,          KEY_RESERVED},
        {ISCL_KEYCODE_STB_INPUT,          KEY_RESERVED},
        {ISCL_KEYCODE_AVR_POWER,          KEY_RESERVED},
        {ISCL_KEYCODE_AVR_INPUT,          KEY_RESERVED},
        {ISCL_KEYCODE_PROG_RED,           KEY_RED},
        {ISCL_KEYCODE_PROG_GREEN,         KEY_GREEN},
        {ISCL_KEYCODE_PROG_YELLOW,        KEY_YELLOW},
        {ISCL_KEYCODE_PROG_BLUE,          KEY_BLUE},
        {ISCL_KEYCODE_APP_SWITCH,         KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_1,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_2,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_3,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_4,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_5,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_6,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_7,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_8,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_9,           KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_10,          KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_11,          KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_12,          KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_13,          KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_14,          KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_15,          KEY_RESERVED},
        {ISCL_KEYCODE_BUTTON_16,          KEY_RESERVED},
        {ISCL_KEYCODE_LANGUAGE_SWITCH,    KEY_RESERVED},
        {ISCL_KEYCODE_MANNER_MODE,        KEY_RESERVED},
        {ISCL_KEYCODE_3D_MODE,            KEY_RESERVED},
        {ISCL_KEYCODE_CONTACTS,           KEY_ADDRESSBOOK},
        {ISCL_KEYCODE_CALENDAR,           KEY_CALENDAR},
        {ISCL_KEYCODE_MUSIC,              KEY_MEDIA},
        {ISCL_KEYCODE_CALCULATOR,         KEY_CALC}
    };
    auto it = s_translation.find(key);
    if (it == s_translation.end()) {
        return KEY_RESERVED;
    }
    return it.value();
}

void HwcomposerBackend::inputEvent(Event *event, void *context)
{
    HwcomposerBackend *backend = reinterpret_cast<HwcomposerBackend*>(context);
    switch (event->type) {
    case KEY_EVENT_TYPE:
        switch (event->action) {
        case ISCL_KEY_EVENT_ACTION_DOWN: {
            const qint32 key = translateKey(event->details.key.key_code);
            if (key == KEY_RESERVED) {
                break;
            }
            if (key == KEY_POWER) {
                // this key is handled internally
                // TODO: trigger timer to decide what should be done: short press/release (un)blank screen
                // long press should emit the normal key pressed
                break;
            }
            QMetaObject::invokeMethod(backend, "keyboardKeyPressed", Qt::QueuedConnection,
                                      Q_ARG(quint32, key),
                                      Q_ARG(quint32, event->details.key.event_time));
            break;
        }
        case ISCL_KEY_EVENT_ACTION_UP: {
            const qint32 key = translateKey(event->details.key.key_code);
            if (key == KEY_RESERVED) {
                break;
            }
            if (key == KEY_POWER) {
                // this key is handled internally
                QMetaObject::invokeMethod(backend, "toggleBlankOutput", Qt::QueuedConnection);
                break;
            }
            QMetaObject::invokeMethod(backend, "keyboardKeyReleased", Qt::QueuedConnection,
                                      Q_ARG(quint32, key),
                                      Q_ARG(quint32, event->details.key.event_time));
            break;
        }
        case ISCL_KEY_EVENT_ACTION_MULTIPLE: // TODO: implement
        default:
            break;
        }
        break;
    case MOTION_EVENT_TYPE: {
        const uint buttonIndex = (event->action & ISCL_MOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> ISCL_MOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        switch (event->action & ISCL_MOTION_EVENT_ACTION_MASK) {
        case ISCL_MOTION_EVENT_ACTION_DOWN:
        case ISCL_MOTION_EVENT_ACTION_POINTER_DOWN:
            QMetaObject::invokeMethod(backend, "touchDown", Qt::QueuedConnection,
                                      Q_ARG(qint32, eventTouchId(event, buttonIndex)),
                                      Q_ARG(QPointF, eventTouchPosition(event, buttonIndex)),
                                      Q_ARG(quint32, event->details.motion.event_time));
            QMetaObject::invokeMethod(backend, "touchFrame", Qt::QueuedConnection);
            break;
        case ISCL_MOTION_EVENT_ACTION_UP:
        case ISCL_MOTION_EVENT_ACTION_POINTER_UP: {
            // first update position - up events can contain additional motion events
            QMetaObject::invokeMethod(backend, "touchMotion", Qt::QueuedConnection,
                                      Q_ARG(qint32, eventTouchId(event, buttonIndex)),
                                      Q_ARG(QPointF, eventTouchPosition(event, buttonIndex)),
                                      Q_ARG(quint32, event->details.motion.event_time));
            QMetaObject::invokeMethod(backend, "touchFrame", Qt::QueuedConnection);

            QMetaObject::invokeMethod(backend, "touchUp", Qt::QueuedConnection,
                                      Q_ARG(qint32, eventTouchId(event, buttonIndex)),
                                      Q_ARG(quint32, event->details.motion.event_time));
            break;
        }
        case ISCL_MOTION_EVENT_ACTION_MOVE:
            //move events affect all pointers
            for (uint i = 0 ; i < event->details.motion.pointer_count ; i++) {
                QMetaObject::invokeMethod(backend, "touchMotion", Qt::QueuedConnection,
                                        Q_ARG(qint32, eventTouchId(event, i)),
                                        Q_ARG(QPointF, eventTouchPosition(event, i)),
                                        Q_ARG(quint32, event->details.motion.event_time));
            }
            QMetaObject::invokeMethod(backend, "touchFrame", Qt::QueuedConnection);
            break;
        case ISCL_MOTION_EVENT_ACTION_CANCEL:
            QMetaObject::invokeMethod(backend, "touchCancel", Qt::QueuedConnection);
            break;
        default:
            // TODO: implement
            break;
        }
        break;
    }
    case HW_SWITCH_EVENT_TYPE:
        qCDebug(KWIN_HWCOMPOSER) << "HW switch event:";
        break;
    }
}

static KWayland::Server::OutputInterface *createOutput(hwc_composer_device_1_t *device)
{
    uint32_t configs[5];
    size_t numConfigs = 5;
    if (device->getDisplayConfigs(device, 0, configs, &numConfigs) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to get hwcomposer display configurations";
        return nullptr;
    }

    int32_t attr_values[4];
    uint32_t attributes[] = {
        HWC_DISPLAY_WIDTH,
        HWC_DISPLAY_HEIGHT,
        HWC_DISPLAY_DPI_X,
        HWC_DISPLAY_DPI_Y,
        HWC_DISPLAY_NO_ATTRIBUTE
    };
    device->getDisplayAttributes(device, 0, configs[0], attributes, attr_values);
    QSize pixel(attr_values[0], attr_values[1]);
    if (pixel.isEmpty()) {
        return nullptr;
    }

    using namespace KWayland::Server;
    OutputInterface *o = waylandServer()->display()->createOutput(waylandServer()->display());
    // TODO: get refresh rate
    o->addMode(pixel, OutputInterface::ModeFlag::Current | OutputInterface::ModeFlag::Preferred);

    if (attr_values[2] != 0 && attr_values[3] != 0) {
         static const qreal factor = 25.4;
         o->setPhysicalSize(QSizeF(qreal(pixel.width() * 1000) / qreal(attr_values[2]) * factor,
                                   qreal(pixel.height() * 1000) / qreal(attr_values[3]) * factor).toSize());
    } else {
         // couldn't read physical size, assume 96 dpi
         o->setPhysicalSize(pixel / 3.8);
    }
    o->create();
    return o;
}

void HwcomposerBackend::init()
{
    hw_module_t *hwcModule = nullptr;
    if (hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t **)&hwcModule) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to get hwcomposer module";
        emit initFailed();
        return;
    }

    hwc_composer_device_1_t *hwcDevice = nullptr;
    if (hwc_open_1(hwcModule, &hwcDevice) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to open hwcomposer device";
        emit initFailed();
        return;
    }

    // unblank, setPowerMode?
    m_device = hwcDevice;
    toggleBlankOutput();

    // get display configuration
    auto output = createOutput(hwcDevice);
    if (!output) {
        emit initFailed();
        return;
    }
    m_displaySize = output->pixelSize();
    qCDebug(KWIN_HWCOMPOSER) << "Display size:" << m_displaySize;

    initInput();

    emit screensQueried();
    setReady(true);
}

void HwcomposerBackend::initInput()
{
    Q_ASSERT(!m_inputListener);
    m_inputListener = new AndroidEventListener;
    m_inputListener->on_new_event = inputEvent;
    m_inputListener->context = this;

    struct InputStackConfiguration config = {
        true,
        10000,
        m_displaySize.width(),
        m_displaySize.height()
    };

    android_input_stack_initialize(m_inputListener, &config);
    android_input_stack_start();

    // we don't know what is really supported, but there is touch
    // and kind of keyboard
    waylandServer()->seat()->setHasPointer(false);
    waylandServer()->seat()->setHasKeyboard(true);
    waylandServer()->seat()->setHasTouch(true);
}

void HwcomposerBackend::toggleBlankOutput()
{
    if (!m_device) {
        return;
    }
    m_outputBlank = !m_outputBlank;
    m_device->blank(m_device, 0, m_outputBlank ? 1 : 0);
    // enable/disable compositor repainting when blanked
    if (Compositor *compositor = Compositor::self()) {
        if (m_outputBlank) {
            compositor->aboutToSwapBuffers();
        } else {
            compositor->bufferSwapComplete();
            compositor->addRepaintFull();
        }
    }
}

HwcomposerWindow *HwcomposerBackend::createSurface()
{
    return new HwcomposerWindow(this);
}

Screens *HwcomposerBackend::createScreens(QObject *parent)
{
    return new HwcomposerScreens(this, parent);
}

OpenGLBackend *HwcomposerBackend::createOpenGLBackend()
{
    return new EglHwcomposerBackend(this);
}

static void initLayer(hwc_layer_1_t *layer, const hwc_rect_t &rect)
{
    memset(layer, 0, sizeof(hwc_layer_1_t));
    layer->compositionType = HWC_FRAMEBUFFER;
    layer->hints = 0;
    layer->flags = 0;
    layer->handle = 0;
    layer->transform = 0;
    layer->blending = HWC_BLENDING_NONE;
    layer->sourceCrop = rect;
    layer->displayFrame = rect;
    layer->visibleRegionScreen.numRects = 1;
    layer->visibleRegionScreen.rects = &layer->displayFrame;
    layer->acquireFenceFd = -1;
    layer->releaseFenceFd = -1;
}

HwcomposerWindow::HwcomposerWindow(HwcomposerBackend *backend)
    : HWComposerNativeWindow(backend->size().width(), backend->size().height(), HAL_PIXEL_FORMAT_RGBA_8888)
    , m_backend(backend)
{
    size_t size = sizeof(hwc_display_contents_1_t) + 2 * sizeof(hwc_layer_1_t);
    hwc_display_contents_1_t *list = (hwc_display_contents_1_t*)malloc(size);
    m_list = (hwc_display_contents_1_t**)malloc(HWC_NUM_DISPLAY_TYPES * sizeof(hwc_display_contents_1_t *));
    for (int i = 0; i < HWC_NUM_DISPLAY_TYPES; ++i) {
        m_list[i] = list;
    }
    const hwc_rect_t rect = {
        0,
        0,
        m_backend->size().width(),
        m_backend->size().height()
    };
    initLayer(&list->hwLayers[0], rect);
    initLayer(&list->hwLayers[1], rect);

    list->retireFenceFd = -1;
    list->flags = HWC_GEOMETRY_CHANGED;
    list->numHwLayers = 2;
}

HwcomposerWindow::~HwcomposerWindow()
{
    // TODO: cleanup
}

static void syncWait(int fd)
{
    if (fd == -1) {
        return;
    }
    sync_wait(fd, -1);
    close(fd);
}

void HwcomposerWindow::present()
{
    HWComposerNativeWindowBuffer *front;
    lockFrontBuffer(&front);

    m_list[0]->hwLayers[1].handle = front->handle;
    m_list[0]->hwLayers[0].handle = NULL;
    m_list[0]->hwLayers[0].flags = HWC_SKIP_LAYER;

    int oldretire = m_list[0]->retireFenceFd;
    int oldrelease = m_list[0]->hwLayers[1].releaseFenceFd;
    int oldrelease2 = m_list[0]->hwLayers[0].releaseFenceFd;

    hwc_composer_device_1_t *device = m_backend->device();
    if (device->prepare(device, 1, m_list) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Error preparing hwcomposer for frame";
    }
    if (device->set(device, 1, m_list) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Error setting device for frame";
    }

    unlockFrontBuffer(front);

    syncWait(oldrelease);
    syncWait(oldrelease2);
    syncWait(oldretire);
}

}

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
#include "wayland_server.h"
// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/seat_interface.h>
// hybris/android
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hybris/input/input_stack_compatibility_layer.h>
#include <hybris/input/input_stack_compatibility_layer_flags_key.h>
#include <hybris/input/input_stack_compatibility_layer_flags_motion.h>

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

static QPointF eventPosition(Event *event)
{
    return QPointF(event->details.motion.pointer_coordinates[0].x,
                   event->details.motion.pointer_coordinates[0].y);
}

void HwcomposerBackend::inputEvent(Event *event, void *context)
{
    HwcomposerBackend *backend = reinterpret_cast<HwcomposerBackend*>(context);
    switch (event->type) {
    case KEY_EVENT_TYPE:
        switch (event->action) {
        case ISCL_KEY_EVENT_ACTION_DOWN:
            // TODO: implement
            break;
        case ISCL_KEY_EVENT_ACTION_UP:
            // TODO: implement
            break;
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
            backend->touchDown(buttonIndex, eventPosition(event), event->details.motion.event_time);
            break;
        case ISCL_MOTION_EVENT_ACTION_UP:
        case ISCL_MOTION_EVENT_ACTION_POINTER_UP:
            // first update position - up events can contain additional motion events
            backend->touchMotion(0, eventPosition(event), event->details.motion.event_time);
            backend->touchFrame();
            backend->touchUp(buttonIndex, event->details.motion.event_time);
            break;
        case ISCL_MOTION_EVENT_ACTION_MOVE:
            // it's always for the first index, other touch points seem not to be provided
            backend->touchMotion(0, eventPosition(event), event->details.motion.event_time);
            backend->touchFrame();
            break;
        case ISCL_MOTION_EVENT_ACTION_CANCEL:
            backend->touchCancel();
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
    hwcDevice->blank(hwcDevice, 0, 0);

    // get display configuration
    auto output = createOutput(hwcDevice);
    if (!output) {
        emit initFailed();
        return;
    }
    m_displaySize = output->pixelSize();
    qCDebug(KWIN_HWCOMPOSER) << "Display size:" << m_displaySize;
    m_device = hwcDevice;

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
    if (device->prepare(device, HWC_NUM_DISPLAY_TYPES, m_list) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Error preparing hwcomposer for frame";
    }
    if (device->set(device, HWC_NUM_DISPLAY_TYPES, m_list) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Error setting device for frame";
    }

    unlockFrontBuffer(front);

    syncWait(oldrelease);
    syncWait(oldrelease2);
    syncWait(oldretire);
}

}

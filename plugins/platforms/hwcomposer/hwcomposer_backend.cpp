/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "egl_hwcomposer_backend.h"
#include "hwcomposer_backend.h"
#include "logging.h"
#include "screens_hwcomposer.h"
#include "composite.h"
#include "input.h"
#include "main.h"
#include "wayland_server.h"
// KWayland
#include <KWaylandServer/output_interface.h>
// KDE
#include <KConfigGroup>
// Qt
#include <QKeyEvent>
#include <QDBusConnection>
// hybris/android
#include <hardware/hardware.h>
#include <hardware/lights.h>
// linux
#include <linux/input.h>

// based on test_hwcomposer.c from libhybris project (Apache 2 licensed)

using namespace KWaylandServer;

namespace KWin
{

BacklightInputEventFilter::BacklightInputEventFilter(HwcomposerBackend *backend)
    : InputEventFilter()
    , m_backend(backend)
{
}

BacklightInputEventFilter::~BacklightInputEventFilter() = default;

bool BacklightInputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    toggleBacklight();
    return true;
}

bool BacklightInputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    toggleBacklight();
    return true;
}

bool BacklightInputEventFilter::keyEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_PowerOff && event->type() == QEvent::KeyRelease) {
        toggleBacklight();
        return true;
    }
    return m_backend->isBacklightOff();
}

bool BacklightInputEventFilter::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(pos)
    Q_UNUSED(time)
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    if (m_touchPoints.isEmpty()) {
        if (!m_doubleTapTimer.isValid()) {
            // this is the first tap
            m_doubleTapTimer.start();
        } else {
            if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
                m_secondTap = true;
            } else {
                // took too long. Let's consider it a new click
                m_doubleTapTimer.restart();
            }
        }
    } else {
        // not a double tap
        m_doubleTapTimer.invalidate();
        m_secondTap = false;
    }
    m_touchPoints << id;
    return true;
}

bool BacklightInputEventFilter::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(time)
    m_touchPoints.removeAll(id);
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    if (m_touchPoints.isEmpty() && m_doubleTapTimer.isValid() && m_secondTap) {
        if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
            toggleBacklight();
        }
        m_doubleTapTimer.invalidate();
        m_secondTap = false;
    }
    return true;
}

bool BacklightInputEventFilter::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
    return m_backend->isBacklightOff();
}

void BacklightInputEventFilter::toggleBacklight()
{
    // queued to not modify the list of event filters while filtering
    QMetaObject::invokeMethod(m_backend, "toggleBlankOutput", Qt::QueuedConnection);
}

HwcomposerBackend::HwcomposerBackend(QObject *parent)
    : Platform(parent)
{
    if (!QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.Solid.PowerManagement"),
                                              QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl"),
                                              QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl"),
                                              QStringLiteral("brightnessChanged"), this,
                                              SLOT(screenBrightnessChanged(int)))) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to connect to brightness control";
    }
}

HwcomposerBackend::~HwcomposerBackend()
{
    if (!m_outputBlank) {
        toggleBlankOutput();
    }
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

    m_hwcVersion = m_device->common.version;
    if ((m_hwcVersion & 0xffff0000) == 0) {
        // Assume header version is always 1
        uint32_t header_version = 1;
        // Legacy version encoding
        m_hwcVersion = (m_hwcVersion << 16) | header_version;
    }

    // register callbacks
    hwc_procs_t *procs = new hwc_procs_t;
    procs->invalidate = [] (const struct hwc_procs* procs) {
        Q_UNUSED(procs)
    };
    procs->vsync = [] (const struct hwc_procs* procs, int disp, int64_t timestamp) {
        Q_UNUSED(procs)
        if (disp != 0) {
            return;
        }
        dynamic_cast<HwcomposerBackend*>(kwinApp()->platform())->wakeVSync();
    };
    procs->hotplug = [] (const struct hwc_procs* procs, int disp, int connected) {
        Q_UNUSED(procs)
        Q_UNUSED(disp)
        Q_UNUSED(connected)
    };
    m_device->registerProcs(m_device, procs);

    //move to HwcomposerOutput + signal

    initLights();
    toggleBlankOutput();
    m_filter.reset(new BacklightInputEventFilter(this));
    input()->prependInputEventFilter(m_filter.data());

    // get display configuration
    m_output.reset(new HwcomposerOutput(hwcDevice));
    if (!m_output->isValid()) {
        emit initFailed();
        return;
    }

    if (m_output->refreshRate() != 0) {
        m_vsyncInterval = 1000000/m_output->refreshRate();
    }

    if (m_lights) {
        using namespace KWaylandServer;

        auto updateDpms = [this] {
            if (!m_output || !m_output->waylandOutput()) {
                m_output->waylandOutput()->setDpmsMode(m_outputBlank ? OutputInterface::DpmsMode::Off : OutputInterface::DpmsMode::On);
            }
        };
        connect(this, &HwcomposerBackend::outputBlankChanged, this, updateDpms);

        connect(m_output.data(), &HwcomposerOutput::dpmsModeRequested, this,
            [this] (KWaylandServer::OutputInterface::DpmsMode mode) {
                if (mode == OutputInterface::DpmsMode::On) {
                    if (m_outputBlank) {
                        toggleBlankOutput();
                    }
                } else {
                    if (!m_outputBlank) {
                        toggleBlankOutput();
                    }
                }
            }
        );
    }

    emit screensQueried();
    setReady(true);
}

QSize HwcomposerBackend::size() const
{
    if (m_output) {
        return m_output->pixelSize();
    }
    return QSize();
}

QSize HwcomposerBackend::screenSize() const
{
    if (m_output) {
        return m_output->pixelSize() / m_output->scale();
    }
    return QSize();
}

int HwcomposerBackend::scale() const
 {
    if (m_output) {
        return m_output->scale();
    }
    return 1;
}

void HwcomposerBackend::initLights()
{
    hw_module_t *lightsModule = nullptr;
    if (hw_get_module(LIGHTS_HARDWARE_MODULE_ID, (const hw_module_t **)&lightsModule) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to get lights module";
        return;
    }
    light_device_t *lightsDevice = nullptr;
    if (lightsModule->methods->open(lightsModule, LIGHT_ID_BACKLIGHT, (hw_device_t **)&lightsDevice) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to create lights device";
        return;
    }
    m_lights = lightsDevice;
}

void HwcomposerBackend::toggleBlankOutput()
{
    if (!m_device) {
        return;
    }
    m_outputBlank = !m_outputBlank;
    toggleScreenBrightness();

#if defined(HWC_DEVICE_API_VERSION_1_4) || defined(HWC_DEVICE_API_VERSION_1_5)
    if (m_hwcVersion > HWC_DEVICE_API_VERSION_1_3)
        m_device->setPowerMode(m_device, 0, m_outputBlank ? HWC_POWER_MODE_OFF : HWC_POWER_MODE_NORMAL);
    else
#endif
        m_device->blank(m_device, 0, m_outputBlank ? 1 : 0);

    // only disable Vsync, enable happens after next frame rendered
    if (m_outputBlank) {
         enableVSync(false);
    }
    // enable/disable compositor repainting when blanked
    setOutputsEnabled(!m_outputBlank);
    if (Compositor *compositor = Compositor::self()) {
        if (!m_outputBlank) {
            compositor->addRepaintFull();
        }
    }
    emit outputBlankChanged();
}

void HwcomposerBackend::toggleScreenBrightness()
{
    if (!m_lights) {
        return;
    }
    const int brightness = m_outputBlank ? 0 : m_oldScreenBrightness;
    struct light_state_t state;
    state.flashMode = LIGHT_FLASH_NONE;
    state.brightnessMode = BRIGHTNESS_MODE_USER;

    state.color = (int)((0xffU << 24) | (brightness << 16) |
                        (brightness << 8) | brightness);
    m_lights->set_light(m_lights, &state);
}

void HwcomposerBackend::enableVSync(bool enable)
{
    if (m_hasVsync == enable) {
        return;
    }
    const int result = m_device->eventControl(m_device, 0, HWC_EVENT_VSYNC, enable ? 1: 0);
    m_hasVsync = enable && (result == 0);
}

HwcomposerWindow *HwcomposerBackend::createSurface()
{
    return new HwcomposerWindow(this);
}

Screens *HwcomposerBackend::createScreens(QObject *parent)
{
    return new HwcomposerScreens(this, parent);
}

Outputs HwcomposerBackend::outputs() const
{
    if (!m_output.isNull()) {
        return QVector<HwcomposerOutput*>({m_output.data()});
    }
    return {};
}

Outputs HwcomposerBackend::enabledOutputs() const
{
    return outputs();
}


OpenGLBackend *HwcomposerBackend::createOpenGLBackend()
{
    return new EglHwcomposerBackend(this);
}

void HwcomposerBackend::waitVSync()
{
    if (!m_hasVsync) {
         return;
    }
    m_vsyncMutex.lock();
    m_vsyncWaitCondition.wait(&m_vsyncMutex, m_vsyncInterval);
    m_vsyncMutex.unlock();
}

void HwcomposerBackend::wakeVSync()
{
    m_vsyncMutex.lock();
    m_vsyncWaitCondition.wakeAll();
    m_vsyncMutex.unlock();
}

static void initLayer(hwc_layer_1_t *layer, const hwc_rect_t &rect, int layerCompositionType)
{
    memset(layer, 0, sizeof(hwc_layer_1_t));
    layer->compositionType = layerCompositionType;
    layer->hints = 0;
    layer->flags = 0;
    layer->handle = 0;
    layer->transform = 0;
    layer->blending = HWC_BLENDING_NONE;
#ifdef HWC_DEVICE_API_VERSION_1_3
    layer->sourceCropf.top = 0.0f;
    layer->sourceCropf.left = 0.0f;
    layer->sourceCropf.bottom = (float) rect.bottom;
    layer->sourceCropf.right = (float) rect.right;
#else
    layer->sourceCrop = rect;
#endif
    layer->displayFrame = rect;
    layer->visibleRegionScreen.numRects = 1;
    layer->visibleRegionScreen.rects = &layer->displayFrame;
    layer->acquireFenceFd = -1;
    layer->releaseFenceFd = -1;
    layer->planeAlpha = 0xFF;
#ifdef HWC_DEVICE_API_VERSION_1_5
    layer->surfaceDamage.numRects = 0;
#endif
}

HwcomposerWindow::HwcomposerWindow(HwcomposerBackend *backend)
    : HWComposerNativeWindow(backend->size().width(), backend->size().height(), HAL_PIXEL_FORMAT_RGBA_8888)
    , m_backend(backend)
{
    setBufferCount(3);

    size_t size = sizeof(hwc_display_contents_1_t) + 2 * sizeof(hwc_layer_1_t);
    hwc_display_contents_1_t *list = (hwc_display_contents_1_t*)malloc(size);
    m_list = (hwc_display_contents_1_t**)malloc(HWC_NUM_DISPLAY_TYPES * sizeof(hwc_display_contents_1_t *));
    for (int i = 0; i < HWC_NUM_DISPLAY_TYPES; ++i) {
        m_list[i] = nullptr;
    }
    // Assign buffer only to the first item, otherwise you get tearing
    // if passed the same to multiple places
    // see https://github.com/mer-hybris/qt5-qpa-hwcomposer-plugin/commit/f1d802151e8a4f5d10d60eb8de8e07552b93a34a
    m_list[0] = list;
    const hwc_rect_t rect = {
        0,
        0,
        m_backend->size().width(),
        m_backend->size().height()
    };
    initLayer(&list->hwLayers[0], rect, HWC_FRAMEBUFFER);
    initLayer(&list->hwLayers[1], rect, HWC_FRAMEBUFFER_TARGET);

    list->retireFenceFd = -1;
    list->flags = HWC_GEOMETRY_CHANGED;
    list->numHwLayers = 2;
}

HwcomposerWindow::~HwcomposerWindow()
{
    // TODO: cleanup
}

void HwcomposerWindow::present(HWComposerNativeWindowBuffer *buffer)
{
    m_backend->waitVSync();
    hwc_composer_device_1_t *device = m_backend->device();

    auto fblayer = &m_list[0]->hwLayers[1];
    fblayer->handle = buffer->handle;
    fblayer->acquireFenceFd = getFenceBufferFd(buffer);
    fblayer->releaseFenceFd = -1;

    int err = device->prepare(device, 1, m_list);
    Q_ASSERT(err == 0);

    err = device->set(device, 1, m_list);
    Q_ASSERT(err == 0);
    m_backend->enableVSync(true);
    setFenceBufferFd(buffer, fblayer->releaseFenceFd);

    if (m_list[0]->retireFenceFd != -1) {
        close(m_list[0]->retireFenceFd);
        m_list[0]->retireFenceFd = -1;
    }
    m_list[0]->flags = 0;
}

HwcomposerOutput::HwcomposerOutput(hwc_composer_device_1_t *device)
    : AbstractWaylandOutput()
    , m_device(device)
{
    uint32_t configs[5];
    size_t numConfigs = 5;
    if (device->getDisplayConfigs(device, 0, configs, &numConfigs) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to get hwcomposer display configurations";
        return;
    }

    int32_t attr_values[5];
    uint32_t attributes[] = {
        HWC_DISPLAY_WIDTH,
        HWC_DISPLAY_HEIGHT,
        HWC_DISPLAY_DPI_X,
        HWC_DISPLAY_DPI_Y,
        HWC_DISPLAY_VSYNC_PERIOD ,
        HWC_DISPLAY_NO_ATTRIBUTE
    };
    device->getDisplayAttributes(device, 0, configs[0], attributes, attr_values);
    QSize pixelSize(attr_values[0], attr_values[1]);
    if (pixelSize.isEmpty()) {
        return;
    }

    QSizeF physicalSize;
    if (attr_values[2] != 0 && attr_values[3] != 0) {
         static const qreal factor = 25.4;
         physicalSize = QSizeF(qreal(pixelSize.width() * 1000) / qreal(attr_values[2]) * factor,
                               qreal(pixelSize.height() * 1000) / qreal(attr_values[3]) * factor);
    } else {
         // couldn't read physical size, assume 96 dpi
         physicalSize = pixelSize / 3.8;
    }

    OutputDeviceInterface::Mode mode;
    mode.id = 0;
    mode.size = pixelSize;
    mode.flags = OutputDeviceInterface::ModeFlag::Current | OutputDeviceInterface::ModeFlag::Preferred;
    mode.refreshRate = (attr_values[4] == 0) ? 60000 : 10E11/attr_values[4];

    initInterfaces(QString(), QString(), QByteArray(), physicalSize.toSize(), {mode});
    setInternal(true);
    setDpmsSupported(true);

    const auto outputGroup = kwinApp()->config()->group("HWComposerOutputs").group("0");
    setScale(outputGroup.readEntry("Scale", 1));
    setWaylandMode(pixelSize, mode.refreshRate);
}

HwcomposerOutput::~HwcomposerOutput()
{
    hwc_close_1(m_device);
}

bool HwcomposerOutput::isValid() const
{
    return isEnabled();
}

void HwcomposerOutput::updateDpms(KWaylandServer::OutputInterface::DpmsMode mode)
{
    emit dpmsModeRequested(mode);
}

}

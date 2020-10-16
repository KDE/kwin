/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef KWIN_HWCOMPOSER_BACKEND_H
#define KWIN_HWCOMPOSER_BACKEND_H
#include "platform.h"
#include "abstract_wayland_output.h"
#include "input.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>

#include <android-config.h>
// libhybris
#include <hardware/hwcomposer.h>
#include <hwcomposer_window.h>
// needed as hwcomposer_window.h includes EGL which on non-arm includes Xlib
#include <fixx11h.h>

typedef struct hwc_display_contents_1 hwc_display_contents_1_t;
typedef struct hwc_layer_1 hwc_layer_1_t;
typedef struct hwc_composer_device_1 hwc_composer_device_1_t;
struct light_device_t;

class HWComposerNativeWindowBuffer;

namespace KWin
{

class HwcomposerWindow;
class BacklightInputEventFilter;

class HwcomposerOutput : public AbstractWaylandOutput
{
    Q_OBJECT
public:
    HwcomposerOutput(hwc_composer_device_1_t *device);
    ~HwcomposerOutput() override;
    bool isValid() const;

    void updateDpms(KWaylandServer::OutputInterface::DpmsMode mode) override;
Q_SIGNALS:
    void dpmsModeRequested(KWaylandServer::OutputInterface::DpmsMode mode);
private:
    QSize m_pixelSize;
    hwc_composer_device_1_t *m_device;
};

class HwcomposerBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "hwcomposer.json")
public:
    explicit HwcomposerBackend(QObject *parent = nullptr);
    virtual ~HwcomposerBackend();

    void init() override;
    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;

    Outputs outputs() const override;
    Outputs enabledOutputs() const override;

    QSize size() const;
    QSize screenSize() const override;

    int scale() const;

    HwcomposerWindow *createSurface();

    hwc_composer_device_1_t *device() const {
        return m_device;
    }
    void enableVSync(bool enable);
    void waitVSync();
    void wakeVSync();

    bool isBacklightOff() const {
        return m_outputBlank;
    }

    QVector<CompositingType> supportedCompositors() const override {
        return QVector<CompositingType>{OpenGLCompositing};
    }

Q_SIGNALS:
    void outputBlankChanged();

private Q_SLOTS:
    void toggleBlankOutput();
    void screenBrightnessChanged(int brightness) {
        m_oldScreenBrightness = brightness;
    }

private:
    void initLights();
    void toggleScreenBrightness();
    hwc_composer_device_1_t *m_device = nullptr;
    light_device_t *m_lights = nullptr;
    bool m_outputBlank = true;
    int m_vsyncInterval = 16;
    uint32_t m_hwcVersion;
    int m_oldScreenBrightness = 0x7f;
    bool m_hasVsync = false;
    QMutex m_vsyncMutex;
    QWaitCondition m_vsyncWaitCondition;
    QScopedPointer<BacklightInputEventFilter> m_filter;
    QScopedPointer<HwcomposerOutput> m_output;
};

class HwcomposerWindow : public HWComposerNativeWindow
{
public:
    virtual ~HwcomposerWindow();

    void present(HWComposerNativeWindowBuffer *buffer);

private:
    friend HwcomposerBackend;
    HwcomposerWindow(HwcomposerBackend *backend);
    HwcomposerBackend *m_backend;
    hwc_display_contents_1_t **m_list;
};

class BacklightInputEventFilter : public InputEventFilter
{
public:
    BacklightInputEventFilter(HwcomposerBackend *backend);
    virtual ~BacklightInputEventFilter();

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

private:
    void toggleBacklight();
    HwcomposerBackend *m_backend;
    QElapsedTimer m_doubleTapTimer;
    QVector<qint32> m_touchPoints;
    bool m_secondTap = false;
};

}

#endif

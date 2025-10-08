/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/drmdevice.h"
#include "core/inputbackend.h"
#include "core/inputdevice.h"
#include "core/outputbackend.h"
#include "utils/filedescriptor.h"

#include <kwin_export.h>

#include <QObject>
#include <QSize>

#include <xcb/render.h>
#include <xcb/xcb.h>

typedef struct _XCBKeySymbols xcb_key_symbols_t;
class NETWinInfo;
class QSocketNotifier;

namespace KWin
{
class X11WindowedBackend;
class X11WindowedOutput;

class X11WindowedInputDevice : public InputDevice
{
    Q_OBJECT

public:
    explicit X11WindowedInputDevice() = default;

    void setPointer(bool set);
    void setKeyboard(bool set);
    void setTouch(bool set);
    void setName(const QString &name);

    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    bool isKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;

private:
    QString m_name;
    bool m_pointer = false;
    bool m_keyboard = false;
    bool m_touch = false;
};

class X11WindowedInputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit X11WindowedInputBackend(X11WindowedBackend *backend);

    void initialize() override;

private:
    X11WindowedBackend *m_backend;
};

struct X11WindowedBackendOptions
{
    QString display;
    int outputCount = 1;
    qreal outputScale = 1;
    QSize outputSize = QSize(1024, 768);
    bool fullscreen = false;
};

class KWIN_EXPORT X11WindowedBackend : public OutputBackend
{
    Q_OBJECT

public:
    explicit X11WindowedBackend(const X11WindowedBackendOptions &options);
    ~X11WindowedBackend() override;

    xcb_connection_t *connection() const;
    xcb_screen_t *screen() const;
    int screenNumer() const;
    xcb_window_t rootWindow() const;
    DrmDevice *drmDevice() const;

    bool hasXInput() const;

    xcb_render_pictformat_t pictureFormatForDepth(int depth) const;

    QHash<uint32_t, QList<uint64_t>> driFormats() const;
    uint32_t driFormatForDepth(int depth) const;
    int driMajorVersion() const;
    int driMinorVersion() const;

    bool initialize() override;
    std::unique_ptr<EglBackend> createOpenGLBackend() override;
    std::unique_ptr<QPainterBackend> createQPainterBackend() override;
    std::unique_ptr<InputBackend> createInputBackend() override;
    QList<CompositingType> supportedCompositors() const override;
    Outputs outputs() const override;

    X11WindowedInputDevice *pointerDevice() const;
    X11WindowedInputDevice *keyboardDevice() const;
    X11WindowedInputDevice *touchDevice() const;

    void setEglDisplay(EglDisplay *display);
    EglDisplay *sceneEglDisplayObject() const override;

private:
    void createOutputs();
    void grabKeyboard(xcb_timestamp_t time);
    void updateWindowTitle();
    void handleEvent(xcb_generic_event_t *event);
    void handleClientMessage(xcb_client_message_event_t *event);
    void handleButtonPress(xcb_button_press_event_t *event);
    void handleExpose(xcb_expose_event_t *event);
    void handleXinputEvent(xcb_ge_generic_event_t *event);
    void handlePresentEvent(xcb_ge_generic_event_t *event);
    void updateSize(xcb_configure_notify_event_t *event);
    void initXInput();
    void initDri3();
    void initRender();
    X11WindowedOutput *findOutput(xcb_window_t window) const;
    void destroyOutputs();

    X11WindowedBackendOptions m_options;
    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_screen = nullptr;
    xcb_key_symbols_t *m_keySymbols = nullptr;
    int m_screenNumber = 0;

    std::unique_ptr<X11WindowedInputDevice> m_pointerDevice;
    std::unique_ptr<X11WindowedInputDevice> m_keyboardDevice;
    std::unique_ptr<X11WindowedInputDevice> m_touchDevice;

    xcb_atom_t m_protocols = XCB_ATOM_NONE;
    xcb_atom_t m_deleteWindowProtocol = XCB_ATOM_NONE;
    bool m_keyboardGrabbed = false;
    std::unique_ptr<QSocketNotifier> m_eventNotifier;

    bool m_hasXInput = false;
    int m_xiOpcode = 0;
    int m_majorVersion = 0;
    int m_minorVersion = 0;

    int m_presentOpcode = 0;
    int m_presentMajorVersion = 0;
    int m_presentMinorVersion = 0;

    bool m_hasShm = false;

    bool m_hasDri = false;
    int m_driMajorVersion = 0;
    int m_driMinorVersion = 0;
    QHash<uint32_t, QList<uint64_t>> m_driFormats;

    std::shared_ptr<DrmDevice> m_drmDevice;
    EglDisplay *m_eglDisplay = nullptr;

    QList<X11WindowedOutput *> m_outputs;
    QHash<int, xcb_render_pictformat_t> m_pictureFormats;
};

} // namespace KWin

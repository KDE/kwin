/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/inputbackend.h"
#include "core/inputdevice.h"
#include "core/platform.h"

#include <kwin_export.h>

#include <QObject>
#include <QSize>

#include <xcb/xcb.h>

struct _XDisplay;
typedef struct _XDisplay Display;
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

    QString sysName() const override;
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    LEDs leds() const override;
    void setLeds(LEDs leds) override;

    bool isKeyboard() const override;
    bool isAlphaNumericKeyboard() const override;
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

class KWIN_EXPORT X11WindowedBackend : public Platform
{
    Q_OBJECT

public:
    explicit X11WindowedBackend();
    ~X11WindowedBackend() override;
    bool initialize() override;

    xcb_connection_t *connection() const
    {
        return m_connection;
    }
    xcb_screen_t *screen() const
    {
        return m_screen;
    }
    int screenNumer() const
    {
        return m_screenNumber;
    }
    xcb_window_t window() const;
    xcb_window_t windowForScreen(Output *output) const;
    Display *display() const
    {
        return m_display;
    }
    xcb_window_t rootWindow() const;
    bool hasXInput() const
    {
        return m_hasXInput;
    }

    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;
    std::unique_ptr<QPainterBackend> createQPainterBackend() override;
    std::unique_ptr<InputBackend> createInputBackend() override;

    QVector<CompositingType> supportedCompositors() const override
    {
        return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
    }

    X11WindowedInputDevice *pointerDevice() const;
    X11WindowedInputDevice *keyboardDevice() const;
    X11WindowedInputDevice *touchDevice() const;

    Outputs outputs() const override;

Q_SIGNALS:
    void sizeChanged();

private:
    void createOutputs();
    void startEventReading();
    void grabKeyboard(xcb_timestamp_t time);
    void updateWindowTitle();
    void handleEvent(xcb_generic_event_t *event);
    void handleClientMessage(xcb_client_message_event_t *event);
    void handleButtonPress(xcb_button_press_event_t *event);
    void handleExpose(xcb_expose_event_t *event);
    void updateSize(xcb_configure_notify_event_t *event);
    void createCursor(const QImage &img, const QPoint &hotspot);
    void initXInput();
    X11WindowedOutput *findOutput(xcb_window_t window) const;

    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_screen = nullptr;
    xcb_key_symbols_t *m_keySymbols = nullptr;
    int m_screenNumber = 0;

    std::unique_ptr<X11WindowedInputDevice> m_pointerDevice;
    std::unique_ptr<X11WindowedInputDevice> m_keyboardDevice;
    std::unique_ptr<X11WindowedInputDevice> m_touchDevice;

    xcb_atom_t m_protocols = XCB_ATOM_NONE;
    xcb_atom_t m_deleteWindowProtocol = XCB_ATOM_NONE;
    xcb_cursor_t m_cursor = XCB_CURSOR_NONE;
    Display *m_display = nullptr;
    bool m_keyboardGrabbed = false;
    std::unique_ptr<QSocketNotifier> m_eventNotifier;

    bool m_hasXInput = false;
    int m_xiOpcode = 0;
    int m_majorVersion = 0;
    int m_minorVersion = 0;

    QVector<X11WindowedOutput *> m_outputs;
};

} // namespace KWin

/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "appmenu_interface.h"
#include "blur_interface.h"
#include "compositor_interface.h"
#include "contrast_interface.h"
#include "datacontroldevicemanager_v1_interface.h"
#include "datadevicemanager_interface.h"
#include "dpms_interface.h"
#include "eglstream_controller_interface.h"
#include "fakeinput_interface.h"
#include "idle_interface.h"
#include "idleinhibit_v1_interface_p.h"
#include "keyboard_shortcuts_inhibit_v1_interface.h"
#include "keystate_interface.h"
#include "layershell_v1_interface.h"
#include "linuxdmabuf_v1_interface.h"
#include "logging.h"
#include "output_interface.h"
#include "outputconfiguration_interface.h"
#include "outputdevice_interface.h"
#include "outputmanagement_interface.h"
#include "plasmashell_interface.h"
#include "plasmavirtualdesktop_interface.h"
#include "plasmawindowmanagement_interface.h"
#include "pointerconstraints_v1_interface_p.h"
#include "pointergestures_v1_interface.h"
#include "primaryselectiondevicemanager_v1_interface.h"
#include "relativepointer_v1_interface.h"
#include "seat_interface.h"
#include "screencast_v1_interface.h"
#include "server_decoration_interface.h"
#include "server_decoration_palette_interface.h"
#include "shadow_interface.h"
#include "slide_interface.h"
#include "subcompositor_interface.h"
#include "tablet_v2_interface.h"
#include "textinput_v2_interface_p.h"
#include "textinput_v3_interface_p.h"
#include "viewporter_interface.h"
#include "xdgdecoration_v1_interface.h"
#include "xdgforeign_v2_interface.h"
#include "xdgoutput_v1_interface.h"
#include "xdgshell_interface.h"
#include "inputmethod_v1_interface.h"

#include <QCoreApplication>
#include <QDebug>
#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QThread>

#include <wayland-server.h>

#include <EGL/egl.h>

namespace KWaylandServer
{

class Display::Private
{
public:
    Private(Display *q);

    void registerSocketName(const QString &socketName);

    QSocketNotifier *socketNotifier = nullptr;
    wl_display *display = nullptr;
    wl_event_loop *loop = nullptr;
    bool running = false;
    QList<OutputInterface*> outputs;
    QList<OutputDeviceInterface*> outputdevices;
    QVector<SeatInterface*> seats;
    QVector<ClientConnection*> clients;
    QStringList socketNames;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;

private:
    Display *q;
};

Display::Private::Private(Display *q)
    : q(q)
{
}

void Display::Private::registerSocketName(const QString &socketName)
{
    socketNames.append(socketName);
    emit q->socketNamesChanged();
}

Display::Display(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
    d->display = wl_display_create();
    d->loop = wl_display_get_event_loop(d->display);
}

Display::~Display()
{
    wl_display_destroy_clients(d->display);
    wl_display_destroy(d->display);
}

bool Display::addSocketFileDescriptor(int fileDescriptor)
{
    if (wl_display_add_socket_fd(d->display, fileDescriptor)) {
        qCWarning(KWAYLAND_SERVER, "Failed to add %d fd to display", fileDescriptor);
        return false;
    }
    return true;
}

bool Display::addSocketName(const QString &name)
{
    if (name.isEmpty()) {
        const char *socket = wl_display_add_socket_auto(d->display);
        if (!socket) {
            qCWarning(KWAYLAND_SERVER, "Failed to find a free display socket");
            return false;
        }
        d->registerSocketName(QString::fromUtf8(socket));
    } else {
        if (wl_display_add_socket(d->display, qPrintable(name))) {
            qCWarning(KWAYLAND_SERVER, "Failed to add %s socket to display", qPrintable(name));
            return false;
        }
        d->registerSocketName(name);
    }
    return true;
}

QStringList Display::socketNames() const
{
    return d->socketNames;
}

bool Display::start()
{
    if (d->running) {
        return true;
    }

    const int fileDescriptor = wl_event_loop_get_fd(d->loop);
    if (fileDescriptor == -1) {
        qCWarning(KWAYLAND_SERVER) << "Did not get the file descriptor for the event loop";
        return false;
    }

    d->socketNotifier = new QSocketNotifier(fileDescriptor, QSocketNotifier::Read, this);
    connect(d->socketNotifier, &QSocketNotifier::activated, this, &Display::dispatchEvents);

    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, &Display::flush);

    d->running = true;
    emit runningChanged(true);

    return true;
}

void Display::dispatchEvents()
{
    if (wl_event_loop_dispatch(d->loop, 0) != 0) {
        qCWarning(KWAYLAND_SERVER) << "Error on dispatching Wayland event loop";
    }
}

void Display::flush()
{
    wl_display_flush_clients(d->display);
}

OutputInterface *Display::createOutput(QObject *parent)
{
    OutputInterface *output = new OutputInterface(this, parent);
    connect(output, &QObject::destroyed, this, [this,output] { d->outputs.removeAll(output); });
    d->outputs << output;
    return output;
}

CompositorInterface *Display::createCompositor(QObject *parent)
{
    return new CompositorInterface(this, parent);
}

OutputDeviceInterface *Display::createOutputDevice(QObject *parent)
{
    OutputDeviceInterface *output = new OutputDeviceInterface(this, parent);
    connect(output, &QObject::destroyed, this, [this,output] { d->outputdevices.removeAll(output); });
    d->outputdevices << output;
    return output;
}

OutputManagementInterface *Display::createOutputManagement(QObject *parent)
{
    return new OutputManagementInterface(this, parent);
}

SeatInterface *Display::createSeat(QObject *parent)
{
    SeatInterface *seat = new SeatInterface(this, parent);
    connect(seat, &QObject::destroyed, this, [this, seat] { d->seats.removeAll(seat); });
    d->seats << seat;
    return seat;
}

SubCompositorInterface *Display::createSubCompositor(QObject *parent)
{
    return new SubCompositorInterface(this, parent);
}

DataDeviceManagerInterface *Display::createDataDeviceManager(QObject *parent)
{
    return new DataDeviceManagerInterface(this, parent);
}

PlasmaShellInterface *Display::createPlasmaShell(QObject* parent)
{
    return new PlasmaShellInterface(this, parent);
}

PlasmaWindowManagementInterface *Display::createPlasmaWindowManagement(QObject *parent)
{
    return new PlasmaWindowManagementInterface(this, parent);
}

IdleInterface *Display::createIdle(QObject *parent)
{
    return new IdleInterface(this, parent);
}

FakeInputInterface *Display::createFakeInput(QObject *parent)
{
    return new FakeInputInterface(this, parent);
}

ShadowManagerInterface *Display::createShadowManager(QObject *parent)
{
    return new ShadowManagerInterface(this, parent);
}

BlurManagerInterface *Display::createBlurManager(QObject *parent)
{
    return new BlurManagerInterface(this, parent);
}

ContrastManagerInterface *Display::createContrastManager(QObject *parent)
{
    return new ContrastManagerInterface(this, parent);
}

SlideManagerInterface *Display::createSlideManager(QObject *parent)
{
    return new SlideManagerInterface(this, parent);
}

DpmsManagerInterface *Display::createDpmsManager(QObject *parent)
{
    return new DpmsManagerInterface(this, parent);
}

ServerSideDecorationManagerInterface *Display::createServerSideDecorationManager(QObject *parent)
{
    return new ServerSideDecorationManagerInterface(this, parent);
}

ScreencastV1Interface *Display::createScreencastV1Interface(QObject *parent)
{
    return new ScreencastV1Interface(this, parent);
}

TextInputManagerV2Interface *Display::createTextInputManagerV2(QObject *parent)
{
    return new TextInputManagerV2Interface(this, parent);
}

TextInputManagerV3Interface *Display::createTextInputManagerV3(QObject *parent)
{
    return new TextInputManagerV3Interface(this, parent);
}

XdgShellInterface *Display::createXdgShell(QObject *parent)
{
    return new XdgShellInterface(this, parent);
}

RelativePointerManagerV1Interface *Display::createRelativePointerManagerV1(QObject *parent)
{
    return new RelativePointerManagerV1Interface(this, parent);
}

PointerGesturesV1Interface *Display::createPointerGesturesV1(QObject *parent)
{
    return new PointerGesturesV1Interface(this, parent);
}

PointerConstraintsV1Interface *Display::createPointerConstraintsV1(QObject *parent)
{
    return new PointerConstraintsV1Interface(this, parent);
}

XdgForeignV2Interface *Display::createXdgForeignV2Interface(QObject *parent)
{
    return new XdgForeignV2Interface(this, parent);
}

IdleInhibitManagerV1Interface *Display::createIdleInhibitManagerV1(QObject *parent)
{
    return new IdleInhibitManagerV1Interface(this, parent);
}

AppMenuManagerInterface *Display::createAppMenuManagerInterface(QObject *parent)
{
    return new AppMenuManagerInterface(this, parent);
}

ServerSideDecorationPaletteManagerInterface *Display::createServerSideDecorationPaletteManager(QObject *parent)
{
    return new ServerSideDecorationPaletteManagerInterface(this, parent);
}

LinuxDmabufUnstableV1Interface *Display::createLinuxDmabufInterface(QObject *parent)
{
    return new LinuxDmabufUnstableV1Interface(this, parent);
}

PlasmaVirtualDesktopManagementInterface *Display::createPlasmaVirtualDesktopManagement(QObject *parent)
{
    return new PlasmaVirtualDesktopManagementInterface(this, parent);
}

XdgOutputManagerV1Interface *Display::createXdgOutputManagerV1(QObject *parent)
{
    return new XdgOutputManagerV1Interface(this, parent);
}

XdgDecorationManagerV1Interface *Display::createXdgDecorationManagerV1(QObject *parent)
{
    return new XdgDecorationManagerV1Interface(this, parent);
}

EglStreamControllerInterface *Display::createEglStreamControllerInterface(QObject *parent)
{
    return new EglStreamControllerInterface(this, parent);
}

KeyStateInterface *Display::createKeyStateInterface(QObject *parent)
{
    return new KeyStateInterface(this, parent);
}

TabletManagerV2Interface *Display::createTabletManagerV2(QObject *parent)
{
    return new TabletManagerV2Interface(this, parent);
}

DataControlDeviceManagerV1Interface *Display::createDataControlDeviceManagerV1(QObject *parent)
{
    return new DataControlDeviceManagerV1Interface(this, parent);
}

KeyboardShortcutsInhibitManagerV1Interface *Display::createKeyboardShortcutsInhibitManagerV1(QObject *parent)
{
    return new KeyboardShortcutsInhibitManagerV1Interface(this, parent);
}

InputMethodV1Interface *Display::createInputMethodInterface(QObject *parent)
{
    return new InputMethodV1Interface(this, parent);
}

ViewporterInterface *Display::createViewporter(QObject *parent)
{
    return new ViewporterInterface(this, parent);
}

PrimarySelectionDeviceManagerV1Interface *Display::createPrimarySelectionDeviceManagerV1(QObject *parent)
{
    return new PrimarySelectionDeviceManagerV1Interface(this, parent);
}

InputPanelV1Interface *Display::createInputPanelInterface(QObject *parent)
{
    return new InputPanelV1Interface(this, parent);
}

LayerShellV1Interface *Display::createLayerShellV1(QObject *parent)
{
    return new LayerShellV1Interface(this, parent);
}

void Display::createShm()
{
    Q_ASSERT(d->display);
    wl_display_init_shm(d->display);
}

void Display::removeOutput(OutputInterface *output)
{
    d->outputs.removeAll(output);
    delete output;
}

void Display::removeOutputDevice(OutputDeviceInterface *output)
{
    d->outputdevices.removeAll(output);
    delete output;
}

quint32 Display::nextSerial()
{
    return wl_display_next_serial(d->display);
}

quint32 Display::serial()
{
    return wl_display_get_serial(d->display);
}

bool Display::isRunning() const
{
    return d->running;
}

Display::operator wl_display*()
{
    return d->display;
}

Display::operator wl_display*() const
{
    return d->display;
}

QList< OutputInterface* > Display::outputs() const
{
    return d->outputs;
}

QList< OutputDeviceInterface* > Display::outputDevices() const
{
    return d->outputdevices;
}

QVector<SeatInterface*> Display::seats() const
{
    return d->seats;
}

ClientConnection *Display::getConnection(wl_client *client)
{
    Q_ASSERT(client);
    auto it = std::find_if(d->clients.constBegin(), d->clients.constEnd(),
        [client](ClientConnection *c) {
            return c->client() == client;
        }
    );
    if (it != d->clients.constEnd()) {
        return *it;
    }
    // no ConnectionData yet, create it
    auto c = new ClientConnection(client, this);
    d->clients << c;
    connect(c, &ClientConnection::disconnected, this,
        [this] (ClientConnection *c) {
            const int index = d->clients.indexOf(c);
            Q_ASSERT(index != -1);
            d->clients.remove(index);
            Q_ASSERT(d->clients.indexOf(c) == -1);
            emit clientDisconnected(c);
        }
    );
    emit clientConnected(c);
    return c;
}

QVector< ClientConnection* > Display::connections() const
{
    return d->clients;
}

ClientConnection *Display::createClient(int fd)
{
    Q_ASSERT(fd != -1);
    Q_ASSERT(d->display);
    wl_client *c = wl_client_create(d->display, fd);
    if (!c) {
        return nullptr;
    }
    return getConnection(c);
}

void Display::setEglDisplay(void *display)
{
    if (d->eglDisplay != EGL_NO_DISPLAY) {
        qCWarning(KWAYLAND_SERVER) << "EGLDisplay cannot be changed";
        return;
    }
    d->eglDisplay = (EGLDisplay)display;
}

void *Display::eglDisplay() const
{
    return d->eglDisplay;
}

}

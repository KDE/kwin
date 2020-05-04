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
#include "datadevicemanager_interface.h"
#include "dpms_interface.h"
#include "eglstream_controller_interface.h"
#include "fakeinput_interface.h"
#include "idle_interface.h"
#include "idleinhibit_interface_p.h"
#include "keyboard_shortcuts_inhibit_interface.h"
#include "keystate_interface.h"
#include "linuxdmabuf_v1_interface.h"
#include "logging.h"
#include "output_interface.h"
#include "outputconfiguration_interface.h"
#include "outputdevice_interface.h"
#include "outputmanagement_interface.h"
#include "plasmashell_interface.h"
#include "plasmavirtualdesktop_interface.h"
#include "plasmawindowmanagement_interface.h"
#include "pointerconstraints_interface_p.h"
#include "pointergestures_interface_p.h"
#include "relativepointer_interface_p.h"
#include "remote_access_interface.h"
#include "seat_interface.h"
#include "server_decoration_interface.h"
#include "server_decoration_palette_interface.h"
#include "shadow_interface.h"
#include "slide_interface.h"
#include "subcompositor_interface.h"
#include "tablet_interface.h"
#include "textinput_interface_p.h"
#include "xdgdecoration_interface.h"
#include "xdgforeign_interface.h"
#include "xdgoutput_interface.h"
#include "xdgshell_stable_interface_p.h"
#include "xdgshell_v5_interface_p.h"
#include "xdgshell_v6_interface_p.h"
#include "xdgdecoration_interface.h"
#include "eglstream_controller_interface.h"
#include "keystate_interface.h"
#include "datacontroldevicemanager_v1_interface.h"

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
    void flush();
    void dispatch();
    void setRunning(bool running);
    void installSocketNotifier();

    wl_display *display = nullptr;
    wl_event_loop *loop = nullptr;
    QString socketName = QStringLiteral("wayland-0");
    bool running = false;
    bool automaticSocketNaming = false;
    QList<OutputInterface*> outputs;
    QList<OutputDeviceInterface*> outputdevices;
    QVector<SeatInterface*> seats;
    QVector<ClientConnection*> clients;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;

private:
    Display *q;
};

Display::Private::Private(Display *q)
    : q(q)
{
}

void Display::Private::installSocketNotifier()
{
    if (!QThread::currentThread()) {
        return;
    }
    int fd = wl_event_loop_get_fd(loop);
    if (fd == -1) {
        qCWarning(KWAYLAND_SERVER) << "Did not get the file descriptor for the event loop";
        return;
    }
    QSocketNotifier *m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, q);
    QObject::connect(m_notifier, &QSocketNotifier::activated, q, [this] { dispatch(); } );
    QObject::connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, q, [this] { flush(); });
    setRunning(true);
}

Display::Display(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
}

Display::~Display()
{
    terminate();
    if (d->display) {
        wl_display_destroy(d->display);
    }
}

void Display::Private::flush()
{
    if (!display || !loop) {
        return;
    }
    wl_display_flush_clients(display);
}

void Display::Private::dispatch()
{
    if (!display || !loop) {
        return;
    }
    if (wl_event_loop_dispatch(loop, 0) != 0) {
        qCWarning(KWAYLAND_SERVER) << "Error on dispatching Wayland event loop";
    }
}

void Display::setSocketName(const QString &name)
{
    if (d->socketName == name) {
        return;
    }
    d->socketName = name;
    emit socketNameChanged(d->socketName);
}

QString Display::socketName() const
{
    return d->socketName;
}

void Display::setAutomaticSocketNaming(bool automaticSocketNaming)
{
    if (d->automaticSocketNaming == automaticSocketNaming) {
        return;
    }
    d->automaticSocketNaming = automaticSocketNaming;
    emit automaticSocketNamingChanged(automaticSocketNaming);
}

bool Display::automaticSocketNaming() const
{
    return d->automaticSocketNaming;
}

void Display::start(StartMode mode)
{
    Q_ASSERT(!d->running);
    Q_ASSERT(!d->display);
    d->display = wl_display_create();
    if (mode == StartMode::ConnectToSocket) {
        if (d->automaticSocketNaming) {
            const char *socket = wl_display_add_socket_auto(d->display);
            if (socket == nullptr) {
                qCWarning(KWAYLAND_SERVER) << "Failed to create Wayland socket";
                return;
            }

            const QString newEffectiveSocketName = QString::fromUtf8(socket);
            if (d->socketName != newEffectiveSocketName) {
                d->socketName = newEffectiveSocketName;
                emit socketNameChanged(d->socketName);
            }
        } else if (wl_display_add_socket(d->display, qPrintable(d->socketName)) != 0) {
            qCWarning(KWAYLAND_SERVER) << "Failed to create Wayland socket";
            return;
        }
    }

    d->loop = wl_display_get_event_loop(d->display);
    d->installSocketNotifier();
}

void Display::startLoop()
{
    Q_ASSERT(!d->running);
    Q_ASSERT(d->display);
    d->installSocketNotifier();
}

void Display::dispatchEvents(int msecTimeout)
{
    Q_ASSERT(d->display);
    if (d->running) {
        d->dispatch();
    } else if (d->loop) {
        wl_event_loop_dispatch(d->loop, msecTimeout);
        wl_display_flush_clients(d->display);
    }
}

void Display::terminate()
{
    if (!d->running) {
        return;
    }
    emit aboutToTerminate();
    wl_display_terminate(d->display);
    wl_display_destroy(d->display);
    d->display = nullptr;
    d->loop = nullptr;
    d->setRunning(false);
}

void Display::Private::setRunning(bool r)
{
    Q_ASSERT(running != r);
    running = r;
    emit q->runningChanged(running);
}

OutputInterface *Display::createOutput(QObject *parent)
{
    OutputInterface *output = new OutputInterface(this, parent);
    connect(output, &QObject::destroyed, this, [this,output] { d->outputs.removeAll(output); });
    connect(this, &Display::aboutToTerminate, output, [this,output] { removeOutput(output); });
    d->outputs << output;
    return output;
}

CompositorInterface *Display::createCompositor(QObject *parent)
{
    CompositorInterface *compositor = new CompositorInterface(this, parent);
    connect(this, &Display::aboutToTerminate, compositor, [compositor] { delete compositor; });
    return compositor;
}

OutputDeviceInterface *Display::createOutputDevice(QObject *parent)
{
    OutputDeviceInterface *output = new OutputDeviceInterface(this, parent);
    connect(output, &QObject::destroyed, this, [this,output] { d->outputdevices.removeAll(output); });
    connect(this, &Display::aboutToTerminate, output, [this,output] { removeOutputDevice(output); });
    d->outputdevices << output;
    return output;
}

OutputManagementInterface *Display::createOutputManagement(QObject *parent)
{
    OutputManagementInterface *om = new OutputManagementInterface(this, parent);
    connect(this, &Display::aboutToTerminate, om, [om] { delete om; });
    return om;
}

SeatInterface *Display::createSeat(QObject *parent)
{
    SeatInterface *seat = new SeatInterface(this, parent);
    connect(seat, &QObject::destroyed, this, [this, seat] { d->seats.removeAll(seat); });
    connect(this, &Display::aboutToTerminate, seat, [seat] { delete seat; });
    d->seats << seat;
    return seat;
}

SubCompositorInterface *Display::createSubCompositor(QObject *parent)
{
    auto c = new SubCompositorInterface(this, parent);
    connect(this, &Display::aboutToTerminate, c, [c] { delete c; });
    return c;
}

DataDeviceManagerInterface *Display::createDataDeviceManager(QObject *parent)
{
    auto m = new DataDeviceManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, m, [m] { delete m; });
    return m;
}

PlasmaShellInterface *Display::createPlasmaShell(QObject* parent)
{
    auto s = new PlasmaShellInterface(this, parent);
    connect(this, &Display::aboutToTerminate, s, [s] { delete s; });
    return s;
}

PlasmaWindowManagementInterface *Display::createPlasmaWindowManagement(QObject *parent)
{
    auto wm = new PlasmaWindowManagementInterface(this, parent);
    connect(this, &Display::aboutToTerminate, wm, [wm] { delete wm; });
    return wm;
}

RemoteAccessManagerInterface *Display::createRemoteAccessManager(QObject *parent)
{
    auto i = new RemoteAccessManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, i, [i] { delete i; });
    return i;
}

IdleInterface *Display::createIdle(QObject *parent)
{
    auto i = new IdleInterface(this, parent);
    connect(this, &Display::aboutToTerminate, i, [i] { delete i; });
    return i;
}

FakeInputInterface *Display::createFakeInput(QObject *parent)
{
    auto i = new FakeInputInterface(this, parent);
    connect(this, &Display::aboutToTerminate, i, [i] { delete i; });
    return i;
}

ShadowManagerInterface *Display::createShadowManager(QObject *parent)
{
    auto s = new ShadowManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, s, [s] { delete s; });
    return s;
}

BlurManagerInterface *Display::createBlurManager(QObject *parent)
{
    auto b = new BlurManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

ContrastManagerInterface *Display::createContrastManager(QObject *parent)
{
    auto b = new ContrastManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

SlideManagerInterface *Display::createSlideManager(QObject *parent)
{
    auto b = new SlideManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

DpmsManagerInterface *Display::createDpmsManager(QObject *parent)
{
    auto d = new DpmsManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
    return d;
}

ServerSideDecorationManagerInterface *Display::createServerSideDecorationManager(QObject *parent)
{
    auto d = new ServerSideDecorationManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
    return d;
}

TextInputManagerInterface *Display::createTextInputManager(const TextInputInterfaceVersion &version, QObject *parent)
{
    TextInputManagerInterface *t = nullptr;
    switch (version) {
    case TextInputInterfaceVersion::UnstableV0:
        t = new TextInputManagerUnstableV0Interface(this, parent);
        break;
    case TextInputInterfaceVersion::UnstableV1:
        // unsupported
        return nullptr;
    case TextInputInterfaceVersion::UnstableV2:
        t = new TextInputManagerUnstableV2Interface(this, parent);
    }
    connect(this, &Display::aboutToTerminate, t, [t] { delete t; });
    return t;
}

XdgShellInterface *Display::createXdgShell(const XdgShellInterfaceVersion &version, QObject *parent)
{
    XdgShellInterface *x = nullptr;
    switch (version) {
    case XdgShellInterfaceVersion::UnstableV5:
        x = new XdgShellV5Interface(this, parent);
        break;
    case XdgShellInterfaceVersion::UnstableV6:
        x = new XdgShellV6Interface(this, parent);
        break;
    case XdgShellInterfaceVersion::Stable:
        x = new XdgShellStableInterface(this, parent);
        break;
    }
    connect(this, &Display::aboutToTerminate, x, [x] { delete x; });
    return x;
}

RelativePointerManagerInterface *Display::createRelativePointerManager(const RelativePointerInterfaceVersion &version, QObject *parent)
{
    RelativePointerManagerInterface *r = nullptr;
    switch (version) {
    case RelativePointerInterfaceVersion::UnstableV1:
        r = new RelativePointerManagerUnstableV1Interface(this, parent);
        break;
    }
    connect(this, &Display::aboutToTerminate, r, [r] { delete r; });
    return r;
}

PointerGesturesInterface *Display::createPointerGestures(const PointerGesturesInterfaceVersion &version, QObject *parent)
{
    PointerGesturesInterface *p = nullptr;
    switch (version) {
    case PointerGesturesInterfaceVersion::UnstableV1:
        p = new PointerGesturesUnstableV1Interface(this, parent);
        break;
    }
    connect(this, &Display::aboutToTerminate, p, [p] { delete p; });
    return p;
}

PointerConstraintsInterface *Display::createPointerConstraints(const PointerConstraintsInterfaceVersion &version, QObject *parent)
{
    PointerConstraintsInterface *p = nullptr;
    switch (version) {
    case PointerConstraintsInterfaceVersion::UnstableV1:
        p = new PointerConstraintsUnstableV1Interface(this, parent);
        break;
    }
    connect(this, &Display::aboutToTerminate, p, [p] { delete p; });
    return p;
}

XdgForeignInterface *Display::createXdgForeignInterface(QObject *parent)
{
    XdgForeignInterface *foreign = new XdgForeignInterface(this, parent);
    connect(this, &Display::aboutToTerminate, foreign, [foreign] { delete foreign; });
    return foreign;
}

IdleInhibitManagerInterface *Display::createIdleInhibitManager(const IdleInhibitManagerInterfaceVersion &version, QObject *parent)
{
    IdleInhibitManagerInterface *i = nullptr;
    switch (version) {
    case IdleInhibitManagerInterfaceVersion::UnstableV1:
        i = new IdleInhibitManagerUnstableV1Interface(this, parent);
        break;
    }
    connect(this, &Display::aboutToTerminate, i, [i] { delete i; });
    return i;
}

AppMenuManagerInterface *Display::createAppMenuManagerInterface(QObject *parent)
{
    auto b = new AppMenuManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

ServerSideDecorationPaletteManagerInterface *Display::createServerSideDecorationPaletteManager(QObject *parent)
{
    auto b = new ServerSideDecorationPaletteManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

LinuxDmabufUnstableV1Interface *Display::createLinuxDmabufInterface(QObject *parent)
{
    auto b = new LinuxDmabufUnstableV1Interface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

PlasmaVirtualDesktopManagementInterface *Display::createPlasmaVirtualDesktopManagement(QObject *parent)
{
    auto b = new PlasmaVirtualDesktopManagementInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

XdgOutputManagerInterface *Display::createXdgOutputManager(QObject *parent)
{
    auto b = new XdgOutputManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, b, [b] { delete b; });
    return b;
}

XdgDecorationManagerInterface *Display::createXdgDecorationManager(XdgShellInterface *shellInterface, QObject *parent)
{
    auto d = new XdgDecorationManagerInterface(this, shellInterface, parent);
    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
    return d;
}

EglStreamControllerInterface *Display::createEglStreamControllerInterface(QObject *parent)
{
    EglStreamControllerInterface *e = new EglStreamControllerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, e, [e] { delete e; });
    return e;
}

KeyStateInterface *Display::createKeyStateInterface(QObject *parent)
{
    auto d = new KeyStateInterface(this, parent);
    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
    return d;
}

TabletManagerInterface *Display::createTabletManagerInterface(QObject *parent)
{
    auto d = new TabletManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
    return d;
}

DataControlDeviceManagerV1Interface *Display::createDataControlDeviceManagerV1(QObject *parent)
{
    auto m = new DataControlDeviceManagerV1Interface(this, parent);
    connect(this, &Display::aboutToTerminate, m, [m] { delete m; });
    return m;
}

KeyboardShortcutsInhibitManagerInterface *Display::createKeyboardShortcutsInhibitManager(QObject *parent)
{
    auto d = new KeyboardShortcutsInhibitManagerInterface(this, parent);
    connect(this, &Display::aboutToTerminate, d, [d] { delete d; });
    return d;
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

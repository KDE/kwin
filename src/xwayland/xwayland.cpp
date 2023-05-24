/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwayland.h"

#include <config-kwin.h>

#include "databridge.h"
#include "dnd.h"
#include "window.h"
#include "xwaylandlauncher.h"
#include "xwldrophandler.h"

#include "core/output.h"
#include "input_event_spy.h"
#include "keyboard_input.h"
#include "main_wayland.h"
#include "utils/common.h"
#include "utils/xcbutils.h"
#include "wayland_server.h"
#include "waylandwindow.h"
#include "workspace.h"
#include "x11eventfilter.h"
#include "xkb.h"
#include "xwayland_logging.h"

#include <KSelectionOwner>
#include <wayland/keyboard_interface.h>
#include <wayland/seat_interface.h>
#include <wayland/surface_interface.h>

#include <QAbstractEventDispatcher>
#include <QDataStream>
#include <QFile>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QSocketNotifier>
#include <QTimer>
#include <QtConcurrentRun>

#include <cerrno>
#include <cstring>
#include <input_event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace KWin
{
namespace Xwl
{

class XrandrEventFilter : public X11EventFilter
{
public:
    explicit XrandrEventFilter(Xwayland *backend);

    bool event(xcb_generic_event_t *event) override;

private:
    Xwayland *const m_backend;
};

XrandrEventFilter::XrandrEventFilter(Xwayland *backend)
    : X11EventFilter(Xcb::Extensions::self()->randrNotifyEvent())
    , m_backend(backend)
{
}

bool XrandrEventFilter::event(xcb_generic_event_t *event)
{
    Q_ASSERT((event->response_type & ~0x80) == Xcb::Extensions::self()->randrNotifyEvent());
    m_backend->updatePrimary();
    return false;
}

class XwaylandInputSpy : public QObject, public KWin::InputEventSpy
{
public:
    XwaylandInputSpy()
    {
        connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::focusedKeyboardSurfaceAboutToChange,
                this, [this](KWaylandServer::SurfaceInterface *newSurface) {
                    auto keyboard = waylandServer()->seat()->keyboard();
                    if (!newSurface) {
                        return;
                    }

                    if (waylandServer()->xWaylandConnection() == newSurface->client()) {
                        // Since this is a spy but the keyboard interface gets its normal sendKey calls through filters,
                        // there can be a mismatch in both states.
                        // This loop makes sure all key press events are reset before we switch back to the
                        // Xwayland client and the state is correctly restored.
                        for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
                            if (it.value() == KWaylandServer::KeyboardKeyState::Pressed) {
                                keyboard->sendKey(it.key(), KWaylandServer::KeyboardKeyState::Released, waylandServer()->xWaylandConnection());
                            }
                        }
                        m_states.clear();
                    }
                });
    }

    void setMode(XwaylandEavesdropsMode mode)
    {
        static const QSet<quint32> modifierKeys = {
            Qt::Key_Control,
            Qt::Key_Shift,
            Qt::Key_Alt,
            Qt::Key_Meta,
        };

        switch (mode) {
        case None:
            m_filter = {};
            break;
        case Modifiers:
            m_filter = [](int key, Qt::KeyboardModifiers) {
                return modifierKeys.contains(key);
            };
            break;
        case Combinations:
            m_filter = [](int key, Qt::KeyboardModifiers m) {
                return m != Qt::NoModifier || modifierKeys.contains(key);
            };
            break;
        case All:
            m_filter = [](int, Qt::KeyboardModifiers) {
                return true;
            };
            break;
        }
    }

    void keyEvent(KWin::KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            return;
        }

        Window *window = workspace()->activeWindow();
        if (!m_filter || !m_filter(event->key(), event->modifiers()) || (window && window->isLockScreen())) {
            return;
        }

        auto keyboard = waylandServer()->seat()->keyboard();
        auto surface = keyboard->focusedSurface();
        if (!surface) {
            return;
        }

        auto client = surface->client();
        if (waylandServer()->xWaylandConnection() != client) {
            KWaylandServer::KeyboardKeyState state{event->type() == QEvent::KeyPress};
            if (!updateKey(event->nativeScanCode(), state)) {
                return;
            }

            auto xkb = input()->keyboard()->xkb();
            keyboard->sendModifiers(xkb->modifierState().depressed,
                                    xkb->modifierState().latched,
                                    xkb->modifierState().locked,
                                    xkb->currentLayout());

            waylandServer()->seat()->keyboard()->sendKey(event->nativeScanCode(), state, waylandServer()->xWaylandConnection());
        }
    }

    bool updateKey(quint32 key, KWaylandServer::KeyboardKeyState state)
    {
        auto it = m_states.find(key);
        if (it == m_states.end()) {
            m_states.insert(key, state);
            return true;
        }
        if (it.value() == state) {
            return false;
        }
        it.value() = state;
        return true;
    }

    QHash<quint32, KWaylandServer::KeyboardKeyState> m_states;
    std::function<bool(int key, Qt::KeyboardModifiers)> m_filter;
};

Xwayland::Xwayland(Application *app)
    : m_app(app)
    , m_launcher(new XwaylandLauncher(this))
{
    connect(m_launcher, &XwaylandLauncher::started, this, &Xwayland::handleXwaylandReady);
    connect(m_launcher, &XwaylandLauncher::finished, this, &Xwayland::handleXwaylandFinished);
    connect(m_launcher, &XwaylandLauncher::errorOccurred, this, &Xwayland::errorOccurred);
}

Xwayland::~Xwayland()
{
    m_launcher->stop();
}

void Xwayland::start()
{
    m_launcher->start();
}

XwaylandLauncher *Xwayland::xwaylandLauncher() const
{
    return m_launcher;
}

void Xwayland::dispatchEvents(DispatchEventsMode mode)
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        qCWarning(KWIN_XWL, "Attempting to dispatch X11 events with no connection");
        return;
    }

    const int connectionError = xcb_connection_has_error(connection);
    if (connectionError) {
        qCWarning(KWIN_XWL, "The X11 connection broke (error %d)", connectionError);
        m_launcher->stop();
        return;
    }

    auto pollEventFunc = mode == DispatchEventsMode::Poll ? xcb_poll_for_event : xcb_poll_for_queued_event;

    while (xcb_generic_event_t *event = pollEventFunc(connection)) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        long result = 0;
#else
        qintptr result = 0;
#endif

        QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
        dispatcher->filterNativeEvent(QByteArrayLiteral("xcb_generic_event_t"), event, &result);
        free(event);
    }

    xcb_flush(connection);
}

void Xwayland::installSocketNotifier()
{
    const int fileDescriptor = xcb_get_file_descriptor(kwinApp()->x11Connection());

    m_socketNotifier = new QSocketNotifier(fileDescriptor, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, [this]() {
        dispatchEvents(DispatchEventsMode::Poll);
    });

    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, [this]() {
        dispatchEvents(DispatchEventsMode::EventQueue);
    });
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, [this]() {
        dispatchEvents(DispatchEventsMode::EventQueue);
    });
}

void Xwayland::uninstallSocketNotifier()
{
    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    disconnect(dispatcher, nullptr, this, nullptr);

    delete m_socketNotifier;
    m_socketNotifier = nullptr;
}

void Xwayland::handleXwaylandFinished()
{
    disconnect(workspace(), &Workspace::outputOrderChanged, this, &Xwayland::updatePrimary);

    delete m_xrandrEventsFilter;
    m_xrandrEventsFilter = nullptr;

    // If Xwayland has crashed, we must deactivate the socket notifier and ensure that no X11
    // events will be dispatched before blocking; otherwise we will simply hang...
    uninstallSocketNotifier();

    m_dataBridge.reset();
    m_selectionOwner.reset();

    destroyX11Connection();
}

void Xwayland::handleXwaylandReady()
{
    if (!createX11Connection()) {
        Q_EMIT errorOccurred();
        return;
    }

    qCInfo(KWIN_XWL) << "Xwayland server started on display" << m_launcher->displayName();

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    m_selectionOwner.reset(new KSelectionOwner("WM_S0", kwinApp()->x11Connection(), kwinApp()->x11RootWindow()));
    connect(m_selectionOwner.get(), &KSelectionOwner::lostOwnership,
            this, &Xwayland::handleSelectionLostOwnership);
    connect(m_selectionOwner.get(), &KSelectionOwner::claimedOwnership,
            this, &Xwayland::handleSelectionClaimedOwnership);
    connect(m_selectionOwner.get(), &KSelectionOwner::failedToClaimOwnership,
            this, &Xwayland::handleSelectionFailedToClaimOwnership);
    m_selectionOwner->claim(true);

    m_dataBridge = std::make_unique<DataBridge>();

    auto env = m_app->processStartupEnvironment();
    env.insert(QStringLiteral("DISPLAY"), m_launcher->displayName());
    env.insert(QStringLiteral("XAUTHORITY"), m_launcher->xauthority());
    qputenv("DISPLAY", m_launcher->displayName().toLatin1());
    qputenv("XAUTHORITY", m_launcher->xauthority().toLatin1());
    m_app->setProcessStartupEnvironment(env);

    connect(workspace(), &Workspace::outputOrderChanged, this, &Xwayland::updatePrimary);
    updatePrimary();

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort

    delete m_xrandrEventsFilter;
    m_xrandrEventsFilter = new XrandrEventFilter(this);

    refreshEavesdropping();
    connect(options, &Options::xwaylandEavesdropsChanged, this, &Xwayland::refreshEavesdropping);
}

void Xwayland::refreshEavesdropping()
{
    if (!waylandServer()->seat()->keyboard()) {
        return;
    }

    const bool enabled = options->xwaylandEavesdrops() != None;
    if (enabled == bool(m_inputSpy)) {
        if (m_inputSpy) {
            m_inputSpy->setMode(options->xwaylandEavesdrops());
        }
        return;
    }

    if (enabled) {
        m_inputSpy.reset(new XwaylandInputSpy);
        input()->installInputEventSpy(m_inputSpy.get());
        m_inputSpy->setMode(options->xwaylandEavesdrops());
    } else {
        input()->uninstallInputEventSpy(m_inputSpy.get());
        m_inputSpy.reset();
    }
}

void Xwayland::updatePrimary()
{
    if (workspace()->outputOrder().empty()) {
        return;
    }
    Xcb::RandR::ScreenResources resources(kwinApp()->x11RootWindow());
    xcb_randr_crtc_t *crtcs = resources.crtcs();
    if (!crtcs) {
        return;
    }

    Output *const primaryOutput = workspace()->outputOrder().front();
    for (int i = 0; i < resources->num_crtcs; ++i) {
        Xcb::RandR::CrtcInfo crtcInfo(crtcs[i], resources->config_timestamp);
        const QRect geometry = crtcInfo.rect();
        if (geometry.topLeft() == primaryOutput->geometry().topLeft()) {
            auto outputs = crtcInfo.outputs();
            if (outputs && crtcInfo->num_outputs > 0) {
                qCDebug(KWIN_XWL) << "Setting primary" << primaryOutput << outputs[0];
                xcb_randr_set_output_primary(kwinApp()->x11Connection(), kwinApp()->x11RootWindow(), outputs[0]);
                break;
            }
        }
    }
}

void Xwayland::handleSelectionLostOwnership()
{
    qCWarning(KWIN_XWL) << "Somebody else claimed ownership of WM_S0. This should never happen!";
    m_launcher->stop();
}

void Xwayland::handleSelectionFailedToClaimOwnership()
{
    qCWarning(KWIN_XWL) << "Failed to claim ownership of WM_S0. This should never happen!";
    m_launcher->stop();
}

void Xwayland::handleSelectionClaimedOwnership()
{
    Q_EMIT started();
}

bool Xwayland::createX11Connection()
{
    xcb_connection_t *connection = xcb_connect_to_fd(m_launcher->xcbConnectionFd(), nullptr);

    const int errorCode = xcb_connection_has_error(connection);
    if (errorCode) {
        qCDebug(KWIN_XWL, "Failed to establish the XCB connection (error %d)", errorCode);
        return false;
    }

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    Q_ASSERT(screen);

    m_app->setX11Connection(connection);
    m_app->setX11RootWindow(screen->root);

    m_app->createAtoms();
    m_app->installNativeX11EventFilter();

    installSocketNotifier();

    // Note that it's very important to have valid x11RootWindow(), and atoms when the
    // rest of kwin is notified about the new X11 connection.
    Q_EMIT m_app->x11ConnectionChanged();

    return true;
}

void Xwayland::destroyX11Connection()
{
    if (!m_app->x11Connection()) {
        return;
    }

    Q_EMIT m_app->x11ConnectionAboutToBeDestroyed();

    Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
    m_app->destroyAtoms();
    m_app->removeNativeX11EventFilter();

    xcb_disconnect(m_app->x11Connection());

    m_app->setX11Connection(nullptr);
    m_app->setX11RootWindow(XCB_WINDOW_NONE);

    Q_EMIT m_app->x11ConnectionChanged();
}

DragEventReply Xwayland::dragMoveFilter(Window *target, const QPoint &pos)
{
    if (m_dataBridge) {
        return m_dataBridge->dragMoveFilter(target, pos);
    } else {
        return DragEventReply::Wayland;
    }
}

KWaylandServer::AbstractDropHandler *Xwayland::xwlDropHandler()
{
    if (m_dataBridge) {
        return m_dataBridge->dnd()->dropHandler();
    } else {
        return nullptr;
    }
}

} // namespace Xwl
} // namespace KWin

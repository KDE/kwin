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
#include <wayland/keyboard.h>
#include <wayland/seat.h>
#include <wayland/surface.h>

#include <QAbstractEventDispatcher>
#include <QDataStream>
#include <QFile>
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
        connect(waylandServer()->seat(), &SeatInterface::focusedKeyboardSurfaceAboutToChange,
                this, [this](SurfaceInterface *newSurface) {
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
                            if (it.value() == KeyboardKeyState::Pressed) {
                                keyboard->sendKey(it.key(), KeyboardKeyState::Released, waylandServer()->xWaylandConnection());
                            }
                        }
                        m_states.clear();
                    }
                });
    }

    void setMode(XwaylandEavesdropsMode mode)
    {
        static const Qt::KeyboardModifiers modifierKeys = {
            Qt::ControlModifier,
            Qt::AltModifier,
            Qt::MetaModifier,
        };

        static const QSet<quint32> characterKeys = {
            Qt::Key_Any,
            Qt::Key_Space,
            Qt::Key_Exclam,
            Qt::Key_QuoteDbl,
            Qt::Key_NumberSign,
            Qt::Key_Dollar,
            Qt::Key_Percent,
            Qt::Key_Ampersand,
            Qt::Key_Apostrophe,
            Qt::Key_ParenLeft,
            Qt::Key_ParenRight,
            Qt::Key_Asterisk,
            Qt::Key_Plus,
            Qt::Key_Comma,
            Qt::Key_Minus,
            Qt::Key_Period,
            Qt::Key_Slash,
            Qt::Key_0,
            Qt::Key_1,
            Qt::Key_2,
            Qt::Key_3,
            Qt::Key_4,
            Qt::Key_5,
            Qt::Key_6,
            Qt::Key_7,
            Qt::Key_8,
            Qt::Key_9,
            Qt::Key_Colon,
            Qt::Key_Semicolon,
            Qt::Key_Less,
            Qt::Key_Equal,
            Qt::Key_Greater,
            Qt::Key_Question,
            Qt::Key_At,
            Qt::Key_A,
            Qt::Key_B,
            Qt::Key_C,
            Qt::Key_D,
            Qt::Key_E,
            Qt::Key_F,
            Qt::Key_G,
            Qt::Key_H,
            Qt::Key_I,
            Qt::Key_J,
            Qt::Key_K,
            Qt::Key_L,
            Qt::Key_M,
            Qt::Key_N,
            Qt::Key_O,
            Qt::Key_P,
            Qt::Key_Q,
            Qt::Key_R,
            Qt::Key_S,
            Qt::Key_T,
            Qt::Key_U,
            Qt::Key_V,
            Qt::Key_W,
            Qt::Key_X,
            Qt::Key_Y,
            Qt::Key_Z,
            Qt::Key_BracketLeft,
            Qt::Key_Backslash,
            Qt::Key_BracketRight,
            Qt::Key_AsciiCircum,
            Qt::Key_Underscore,
            Qt::Key_QuoteLeft,
            Qt::Key_BraceLeft,
            Qt::Key_Bar,
            Qt::Key_BraceRight,
            Qt::Key_AsciiTilde,
            Qt::Key_nobreakspace,
            Qt::Key_exclamdown,
            Qt::Key_cent,
            Qt::Key_sterling,
            Qt::Key_currency,
            Qt::Key_yen,
            Qt::Key_brokenbar,
            Qt::Key_section,
            Qt::Key_diaeresis,
            Qt::Key_copyright,
            Qt::Key_ordfeminine,
            Qt::Key_guillemotleft,
            Qt::Key_notsign,
            Qt::Key_hyphen,
            Qt::Key_registered,
            Qt::Key_macron,
            Qt::Key_degree,
            Qt::Key_plusminus,
            Qt::Key_twosuperior,
            Qt::Key_threesuperior,
            Qt::Key_acute,
            Qt::Key_mu,
            Qt::Key_paragraph,
            Qt::Key_periodcentered,
            Qt::Key_cedilla,
            Qt::Key_onesuperior,
            Qt::Key_masculine,
            Qt::Key_guillemotright,
            Qt::Key_onequarter,
            Qt::Key_onehalf,
            Qt::Key_threequarters,
            Qt::Key_questiondown,
            Qt::Key_Agrave,
            Qt::Key_Aacute,
            Qt::Key_Acircumflex,
            Qt::Key_Atilde,
            Qt::Key_Adiaeresis,
            Qt::Key_Aring,
            Qt::Key_AE,
            Qt::Key_Ccedilla,
            Qt::Key_Egrave,
            Qt::Key_Eacute,
            Qt::Key_Ecircumflex,
            Qt::Key_Ediaeresis,
            Qt::Key_Igrave,
            Qt::Key_Iacute,
            Qt::Key_Icircumflex,
            Qt::Key_Idiaeresis,
            Qt::Key_ETH,
            Qt::Key_Ntilde,
            Qt::Key_Ograve,
            Qt::Key_Oacute,
            Qt::Key_Ocircumflex,
            Qt::Key_Otilde,
            Qt::Key_Odiaeresis,
            Qt::Key_multiply,
            Qt::Key_Ooblique,
            Qt::Key_Ugrave,
            Qt::Key_Uacute,
            Qt::Key_Ucircumflex,
            Qt::Key_Udiaeresis,
            Qt::Key_Yacute,
            Qt::Key_THORN,
            Qt::Key_ssharp,
            Qt::Key_division,
            Qt::Key_ydiaeresis,
            Qt::Key_Multi_key,
            Qt::Key_Codeinput,
            Qt::Key_SingleCandidate,
            Qt::Key_MultipleCandidate,
            Qt::Key_PreviousCandidate,
            Qt::Key_Mode_switch,
            Qt::Key_Kanji,
            Qt::Key_Muhenkan,
            Qt::Key_Henkan,
            Qt::Key_Romaji,
            Qt::Key_Hiragana,
            Qt::Key_Katakana,
            Qt::Key_Hiragana_Katakana,
            Qt::Key_Zenkaku,
            Qt::Key_Hankaku,
            Qt::Key_Zenkaku_Hankaku,
            Qt::Key_Touroku,
            Qt::Key_Massyo,
            Qt::Key_Kana_Lock,
            Qt::Key_Kana_Shift,
            Qt::Key_Eisu_Shift,
            Qt::Key_Eisu_toggle,
            Qt::Key_Hangul,
            Qt::Key_Hangul_Start,
            Qt::Key_Hangul_End,
            Qt::Key_Hangul_Hanja,
            Qt::Key_Hangul_Jamo,
            Qt::Key_Hangul_Romaja,
            Qt::Key_Hangul_Jeonja,
            Qt::Key_Hangul_Banja,
            Qt::Key_Hangul_PreHanja,
            Qt::Key_Hangul_PostHanja,
            Qt::Key_Hangul_Special,
            Qt::Key_Dead_Grave,
            Qt::Key_Dead_Acute,
            Qt::Key_Dead_Circumflex,
            Qt::Key_Dead_Tilde,
            Qt::Key_Dead_Macron,
            Qt::Key_Dead_Breve,
            Qt::Key_Dead_Abovedot,
            Qt::Key_Dead_Diaeresis,
            Qt::Key_Dead_Abovering,
            Qt::Key_Dead_Doubleacute,
            Qt::Key_Dead_Caron,
            Qt::Key_Dead_Cedilla,
            Qt::Key_Dead_Ogonek,
            Qt::Key_Dead_Iota,
            Qt::Key_Dead_Voiced_Sound,
            Qt::Key_Dead_Semivoiced_Sound,
            Qt::Key_Dead_Belowdot,
            Qt::Key_Dead_Hook,
            Qt::Key_Dead_Horn,
            Qt::Key_Dead_Stroke,
            Qt::Key_Dead_Abovecomma,
            Qt::Key_Dead_Abovereversedcomma,
            Qt::Key_Dead_Doublegrave,
            Qt::Key_Dead_Belowring,
            Qt::Key_Dead_Belowmacron,
            Qt::Key_Dead_Belowcircumflex,
            Qt::Key_Dead_Belowtilde,
            Qt::Key_Dead_Belowbreve,
            Qt::Key_Dead_Belowdiaeresis,
            Qt::Key_Dead_Invertedbreve,
            Qt::Key_Dead_Belowcomma,
            Qt::Key_Dead_Currency,
            Qt::Key_Dead_a,
            Qt::Key_Dead_A,
            Qt::Key_Dead_e,
            Qt::Key_Dead_E,
            Qt::Key_Dead_i,
            Qt::Key_Dead_I,
            Qt::Key_Dead_o,
            Qt::Key_Dead_O,
            Qt::Key_Dead_u,
            Qt::Key_Dead_U,
            Qt::Key_Dead_Small_Schwa,
            Qt::Key_Dead_Capital_Schwa,
            Qt::Key_Dead_Greek,
            Qt::Key_Dead_Lowline,
            Qt::Key_Dead_Aboveverticalline,
            Qt::Key_Dead_Belowverticalline};

        switch (mode) {
        case None:
            m_filter = {};
            break;
        case NonCharacterKeys:
            m_filter = [](int key, Qt::KeyboardModifiers) {
                return !characterKeys.contains(key);
            };
            break;
        case AllKeysWithModifier:
            m_filter = [](int key, Qt::KeyboardModifiers m) {
                return m.testAnyFlags(modifierKeys) || !characterKeys.contains(key);
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

        ClientConnection *client = surface->client();
        ClientConnection *xwaylandClient = waylandServer()->xWaylandConnection();
        if (xwaylandClient && xwaylandClient != client) {
            KeyboardKeyState state{event->type() == QEvent::KeyPress};
            if (!updateKey(event->nativeScanCode(), state)) {
                return;
            }

            auto xkb = input()->keyboard()->xkb();
            keyboard->sendModifiers(xkb->modifierState().depressed,
                                    xkb->modifierState().latched,
                                    xkb->modifierState().locked,
                                    xkb->currentLayout());

            waylandServer()->seat()->keyboard()->sendKey(event->nativeScanCode(), state, xwaylandClient);
        }
    }

    bool updateKey(quint32 key, KeyboardKeyState state)
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

    QHash<quint32, KeyboardKeyState> m_states;
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

void Xwayland::init()
{
    m_launcher->enable();

    auto env = m_app->processStartupEnvironment();
    env.insert(QStringLiteral("DISPLAY"), m_launcher->displayName());
    env.insert(QStringLiteral("XAUTHORITY"), m_launcher->xauthority());
    qputenv("DISPLAY", m_launcher->displayName().toLatin1());
    qputenv("XAUTHORITY", m_launcher->xauthority().toLatin1());
    m_app->setProcessStartupEnvironment(env);
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
        qintptr result = 0;

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
    m_compositingManagerSelectionOwner.reset();
    m_windowManagerSelectionOwner.reset();

    m_inputSpy.reset();
    disconnect(options, &Options::xwaylandEavesdropsChanged, this, &Xwayland::refreshEavesdropping);

    destroyX11Connection();
}

void Xwayland::handleXwaylandReady()
{
    if (!createX11Connection()) {
        Q_EMIT errorOccurred();
        return;
    }

    qCInfo(KWIN_XWL) << "Xwayland server started on display" << m_launcher->displayName();

    m_compositingManagerSelectionOwner = std::make_unique<KSelectionOwner>("_NET_WM_CM_S0", kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
    m_compositingManagerSelectionOwner->claim(true);

    xcb_composite_redirect_subwindows(kwinApp()->x11Connection(),
                                      kwinApp()->x11RootWindow(),
                                      XCB_COMPOSITE_REDIRECT_MANUAL);

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    m_windowManagerSelectionOwner = std::make_unique<KSelectionOwner>("WM_S0", kwinApp()->x11Connection(), kwinApp()->x11RootWindow());
    connect(m_windowManagerSelectionOwner.get(), &KSelectionOwner::lostOwnership,
            this, &Xwayland::handleSelectionLostOwnership);
    connect(m_windowManagerSelectionOwner.get(), &KSelectionOwner::claimedOwnership,
            this, &Xwayland::handleSelectionClaimedOwnership);
    connect(m_windowManagerSelectionOwner.get(), &KSelectionOwner::failedToClaimOwnership,
            this, &Xwayland::handleSelectionFailedToClaimOwnership);
    m_windowManagerSelectionOwner->claim(true);

    m_dataBridge = std::make_unique<DataBridge>();

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
        m_inputSpy = std::make_unique<XwaylandInputSpy>();
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
    const QRect primaryOutputGeometry = Xcb::toXNative(primaryOutput->geometryF());
    for (int i = 0; i < resources->num_crtcs; ++i) {
        Xcb::RandR::CrtcInfo crtcInfo(crtcs[i], resources->config_timestamp);
        const QRect geometry = crtcInfo.rect();
        if (geometry.topLeft() == primaryOutputGeometry.topLeft()) {
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

DragEventReply Xwayland::dragMoveFilter(Window *target)
{
    if (m_dataBridge) {
        return m_dataBridge->dragMoveFilter(target);
    } else {
        return DragEventReply::Wayland;
    }
}

AbstractDropHandler *Xwayland::xwlDropHandler()
{
    if (m_dataBridge) {
        return m_dataBridge->dnd()->dropHandler();
    } else {
        return nullptr;
    }
}

} // namespace Xwl
} // namespace KWin

#include "moc_xwayland.cpp"

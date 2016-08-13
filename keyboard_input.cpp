/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "keyboard_input.h"
#include "input_event.h"
#include "abstract_client.h"
#include "options.h"
#include "utils.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
// KWayland
#include <KWayland/Server/datadevice_interface.h>
#include <KWayland/Server/seat_interface.h>
//screenlocker
#include <KScreenLocker/KsldApp>
// Frameworks
#include <KKeyServer>
#include <KGlobalAccel>
// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QKeyEvent>
#include <QTemporaryFile>
// xkbcommon
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
// system
#include <sys/mman.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(KWIN_XKB, "kwin_xkbcommon", QtCriticalMsg)

namespace KWin
{

static void xkbLogHandler(xkb_context *context, xkb_log_level priority, const char *format, va_list args)
{
    Q_UNUSED(context)
    char buf[1024];
    if (std::vsnprintf(buf, 1023, format, args) <= 0) {
        return;
    }
    switch (priority) {
    case XKB_LOG_LEVEL_DEBUG:
        qCDebug(KWIN_XKB) << "XKB:" << buf;
        break;
    case XKB_LOG_LEVEL_INFO:
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
        qCInfo(KWIN_XKB) << "XKB:" << buf;
#endif
        break;
    case XKB_LOG_LEVEL_WARNING:
        qCWarning(KWIN_XKB) << "XKB:" << buf;
        break;
    case XKB_LOG_LEVEL_ERROR:
    case XKB_LOG_LEVEL_CRITICAL:
    default:
        qCCritical(KWIN_XKB) << "XKB:" << buf;
        break;
    }
}

Xkb::Xkb(InputRedirection *input)
    : m_input(input)
    , m_context(xkb_context_new(static_cast<xkb_context_flags>(0)))
    , m_keymap(NULL)
    , m_state(NULL)
    , m_shiftModifier(0)
    , m_controlModifier(0)
    , m_altModifier(0)
    , m_metaModifier(0)
    , m_modifiers(Qt::NoModifier)
{
    if (!m_context) {
        qCDebug(KWIN_XKB) << "Could not create xkb context";
    } else {
        xkb_context_set_log_level(m_context, XKB_LOG_LEVEL_DEBUG);
        xkb_context_set_log_fn(m_context, &xkbLogHandler);
    }

    auto resetModOnlyShortcut = [this] {
        m_modOnlyShortcut.modifier = Qt::NoModifier;
    };
    QObject::connect(m_input, &InputRedirection::pointerButtonStateChanged, resetModOnlyShortcut);
    QObject::connect(m_input, &InputRedirection::pointerAxisChanged, resetModOnlyShortcut);
}

Xkb::~Xkb()
{
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);
    xkb_context_unref(m_context);
}

void Xkb::reconfigure()
{
    if (!m_context) {
        return;
    }

    xkb_keymap *keymap = loadKeymapFromConfig();
    if (!keymap) {
        qCDebug(KWIN_XKB) << "Could not create xkb keymap from configuration";
        keymap = loadDefaultKeymap();
    }
    if (keymap) {
        updateKeymap(keymap);
    } else {
        qCDebug(KWIN_XKB) << "Could not create default xkb keymap";
    }
}

xkb_keymap *Xkb::loadKeymapFromConfig()
{
    // load config
    const KConfigGroup config = KSharedConfig::openConfig(QStringLiteral("kxkbrc"), KConfig::NoGlobals)->group("Layout");
    const QByteArray model = config.readEntry("Model", "pc104").toLocal8Bit();
    const QByteArray layout = config.readEntry("LayoutList", "").toLocal8Bit();
    const QByteArray options = config.readEntry("Options", "").toLocal8Bit();

    xkb_rule_names ruleNames = {
        .rules = nullptr,
        .model = model.constData(),
        .layout = layout.constData(),
        .variant = nullptr,
        .options = options.constData()
    };
    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

xkb_keymap *Xkb::loadDefaultKeymap()
{
    return xkb_keymap_new_from_names(m_context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

void Xkb::installKeymap(int fd, uint32_t size)
{
    if (!m_context) {
        return;
    }
    char *map = reinterpret_cast<char*>(mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));
    if (map == MAP_FAILED) {
        return;
    }
    xkb_keymap *keymap = xkb_keymap_new_from_string(m_context, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_MAP_COMPILE_PLACEHOLDER);
    munmap(map, size);
    if (!keymap) {
        qCDebug(KWIN_XKB) << "Could not map keymap from file";
        return;
    }
    updateKeymap(keymap);
}

void Xkb::updateKeymap(xkb_keymap *keymap)
{
    Q_ASSERT(keymap);
    xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        qCDebug(KWIN_XKB) << "Could not create XKB state";
        xkb_keymap_unref(keymap);
        return;
    }
    // now release the old ones
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);

    m_keymap = keymap;
    m_state = state;

    m_shiftModifier   = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_SHIFT);
    m_controlModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CTRL);
    m_altModifier     = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_ALT);
    m_metaModifier    = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_LOGO);
    m_currentLayout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);

    createKeymapFile();
}

void Xkb::createKeymapFile()
{
    if (!waylandServer()) {
        return;
    }
    // TODO: uninstall keymap on server?
    if (!m_keymap) {
        return;
    }

    ScopedCPointer<char> keymapString(xkb_keymap_get_as_string(m_keymap, XKB_KEYMAP_FORMAT_TEXT_V1));
    if (keymapString.isNull()) {
        return;
    }
    const uint size = qstrlen(keymapString.data()) + 1;

    QTemporaryFile *tmp = new QTemporaryFile(m_input);
    if (!tmp->open()) {
        delete tmp;
        return;
    }
    unlink(tmp->fileName().toUtf8().constData());
    if (!tmp->resize(size)) {
        delete tmp;
        return;
    }
    uchar *address = tmp->map(0, size);
    if (!address) {
        return;
    }
    if (qstrncpy(reinterpret_cast<char*>(address), keymapString.data(), size) == nullptr) {
        delete tmp;
        return;
    }
    waylandServer()->seat()->setKeymap(tmp->handle(), size);
}

void Xkb::updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_mask(m_state, modsDepressed, modsLatched, modsLocked, 0, 0, group);
    updateModifiers();
}

void Xkb::updateKey(uint32_t key, InputRedirection::KeyboardKeyState state)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_key(m_state, key + 8, static_cast<xkb_key_direction>(state));
    updateModifiers();
    if (state == InputRedirection::KeyboardKeyPressed) {
        m_modOnlyShortcut.pressCount++;
        if (m_modOnlyShortcut.pressCount == 1 &&
            m_input->qtButtonStates() == Qt::NoButton) {
            m_modOnlyShortcut.modifier = Qt::KeyboardModifier(int(m_modifiers));
        } else {
            m_modOnlyShortcut.modifier = Qt::NoModifier;
        }
    } else {
        m_modOnlyShortcut.pressCount--;
        // TODO: ignore on lock screen
        if (m_modOnlyShortcut.pressCount == 0) {
            if (m_modOnlyShortcut.modifier != Qt::NoModifier) {
                const auto list = options->modifierOnlyDBusShortcut(m_modOnlyShortcut.modifier);
                if (list.size() >= 4) {
                    auto call = QDBusMessage::createMethodCall(list.at(0), list.at(1), list.at(2), list.at(3));
                    QVariantList args;
                    for (int i = 4; i < list.size(); ++i) {
                        args << list.at(i);
                    }
                    call.setArguments(args);
                    QDBusConnection::sessionBus().asyncCall(call);
                }
            }
        }
        m_modOnlyShortcut.modifier = Qt::NoModifier;
    }
}

void Xkb::updateModifiers()
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::MetaModifier;
    }
    m_modifiers = mods;
    const xkb_layout_index_t layout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);
    if (layout != m_currentLayout) {
        m_currentLayout = layout;
        // notify OSD service about the new layout
        QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"),
        QStringLiteral("org.kde.osdService"),
        QStringLiteral("kbdLayoutChanged"));

        msg << QString::fromLocal8Bit(xkb_keymap_layout_get_name(m_keymap, layout));

        QDBusConnection::sessionBus().asyncCall(msg);
    }
    waylandServer()->seat()->updateKeyboardModifiers(xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED)),
                                                     xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED)),
                                                     xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED)),
                                                     layout);
}

xkb_keysym_t Xkb::toKeysym(uint32_t key)
{
    if (!m_state) {
        return XKB_KEY_NoSymbol;
    }
    return xkb_state_key_get_one_sym(m_state, key + 8);
}

QString Xkb::toString(xkb_keysym_t keysym)
{
    if (!m_state || keysym == XKB_KEY_NoSymbol) {
        return QString();
    }
    QByteArray byteArray(7, 0);
    int ok = xkb_keysym_to_utf8(keysym, byteArray.data(), byteArray.size());
    if (ok == -1 || ok == 0) {
        return QString();
    }
    return QString::fromUtf8(byteArray.constData());
}

Qt::Key Xkb::toQtKey(xkb_keysym_t keysym)
{
    int key = Qt::Key_unknown;
    KKeyServer::symXToKeyQt(keysym, &key);
    return static_cast<Qt::Key>(key);
}

bool Xkb::shouldKeyRepeat(quint32 key) const
{
    if (!m_keymap) {
        return false;
    }
    return xkb_keymap_key_repeats(m_keymap, key) != 0;
}

void Xkb::switchToNextLayout()
{
    if (!m_keymap || !m_state) {
        return;
    }
    const xkb_layout_index_t numLayouts = xkb_keymap_num_layouts(m_keymap);
    const xkb_layout_index_t nextLayout = (xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE) + 1) % numLayouts;
    const xkb_mod_mask_t depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    const xkb_mod_mask_t latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    const xkb_mod_mask_t locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
    xkb_state_update_mask(m_state, depressed, latched, locked, 0, 0, nextLayout);
    updateModifiers();
}

KeyboardInputRedirection::KeyboardInputRedirection(InputRedirection *parent)
    : QObject(parent)
    , m_input(parent)
    , m_xkb(new Xkb(parent))
{
}

KeyboardInputRedirection::~KeyboardInputRedirection()
{
    qDeleteAll(m_repeatTimers);
    m_repeatTimers.clear();
}

void KeyboardInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;

    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(workspace(), &Workspace::clientActivated, this,
        [this] {
            disconnect(m_activeClientSurfaceChangedConnection);
            if (auto c = workspace()->activeClient()) {
                m_activeClientSurfaceChangedConnection = connect(c, &Toplevel::surfaceChanged, this, &KeyboardInputRedirection::update);
            } else {
                m_activeClientSurfaceChangedConnection = QMetaObject::Connection();
            }
            update();
        }
    );
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &KeyboardInputRedirection::update);
    }

    QAction *switchKeyboardAction = new QAction(this);
    switchKeyboardAction->setObjectName(QStringLiteral("Switch to Next Keyboard Layout"));
    switchKeyboardAction->setProperty("componentName", QStringLiteral("KDE Keyboard Layout Switcher"));
    const QKeySequence sequence = QKeySequence(Qt::ALT+Qt::CTRL+Qt::Key_K);
    KGlobalAccel::self()->setDefaultShortcut(switchKeyboardAction, QList<QKeySequence>({sequence}));
    KGlobalAccel::self()->setShortcut(switchKeyboardAction, QList<QKeySequence>({sequence}));
    m_input->registerShortcut(sequence, switchKeyboardAction);
    connect(switchKeyboardAction, &QAction::triggered, this,
        [this] {
            m_xkb->switchToNextLayout();
        }
    );

    QDBusConnection::sessionBus().connect(QString(),
                                          QStringLiteral("/Layouts"),
                                          QStringLiteral("org.kde.keyboard"),
                                          QStringLiteral("reloadConfig"),
                                          this,
                                          SLOT(reconfigure()));

    m_xkb->reconfigure();
}

void KeyboardInputRedirection::reconfigure()
{
    m_xkb->reconfigure();
}

void KeyboardInputRedirection::update()
{
    if (!m_inited) {
        return;
    }
    auto seat = waylandServer()->seat();
    // TODO: this needs better integration
    Toplevel *found = nullptr;
    if (waylandServer()->isScreenLocked()) {
        const ToplevelList &stacking = Workspace::self()->stackingOrder();
        if (!stacking.isEmpty()) {
            auto it = stacking.end();
            do {
                --it;
                Toplevel *t = (*it);
                if (t->isDeleted()) {
                    // a deleted window doesn't get mouse events
                    continue;
                }
                if (!t->isLockScreen()) {
                    continue;
                }
                if (!t->readyForPainting()) {
                    continue;
                }
                found = t;
                break;
            } while (it != stacking.begin());
        }
    } else {
        found = workspace()->activeClient();
    }
    if (found && found->surface()) {
        if (found->surface() != seat->focusedKeyboardSurface()) {
            seat->setFocusedKeyboardSurface(found->surface());
            auto newKeyboard = seat->focusedKeyboard();
            if (newKeyboard && newKeyboard->client() == waylandServer()->xWaylandConnection()) {
                // focus passed to an XWayland surface
                const auto selection = seat->selection();
                auto xclipboard = waylandServer()->xclipboardSyncDataDevice();
                if (xclipboard && selection != xclipboard.data()) {
                    if (selection) {
                        xclipboard->sendSelection(selection);
                    } else {
                        xclipboard->sendClearSelection();
                    }
                }
            }
        }
    } else {
        seat->setFocusedKeyboardSurface(nullptr);
    }
}

void KeyboardInputRedirection::processKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time, LibInput::Device *device)
{
    if (!m_inited) {
        return;
    }
    QEvent::Type type;
    bool autoRepeat = false;
    switch (state) {
    case InputRedirection::KeyboardKeyAutoRepeat:
        autoRepeat = true;
        // fall through
    case InputRedirection::KeyboardKeyPressed:
        type = QEvent::KeyPress;
        break;
    case InputRedirection::KeyboardKeyReleased:
        type = QEvent::KeyRelease;
        break;
    default:
        Q_UNREACHABLE();
    }

    if (!autoRepeat) {
        emit m_input->keyStateChanged(key, state);
        const Qt::KeyboardModifiers oldMods = modifiers();
        m_xkb->updateKey(key, state);
        if (oldMods != modifiers()) {
            emit m_input->keyboardModifiersChanged(modifiers(), oldMods);
        }
    }

    const xkb_keysym_t keySym = m_xkb->toKeysym(key);
    KeyEvent event(type,
                   m_xkb->toQtKey(keySym),
                   m_xkb->modifiers(),
                   key,
                   keySym,
                   m_xkb->toString(m_xkb->toKeysym(key)),
                   autoRepeat,
                   time,
                   device);
    if (state == InputRedirection::KeyboardKeyPressed) {
        if (m_xkb->shouldKeyRepeat(key) && waylandServer()->seat()->keyRepeatDelay() != 0) {
            QTimer *timer = new QTimer;
            timer->setInterval(waylandServer()->seat()->keyRepeatDelay());
            connect(timer, &QTimer::timeout, this,
                [this, timer, time, key] {
                    const int delay = 1000 / waylandServer()->seat()->keyRepeatRate();
                    if (timer->interval() != delay) {
                        timer->setInterval(delay);
                    }
                    // TODO: better time
                    processKey(key, InputRedirection::KeyboardKeyAutoRepeat, time);
                }
            );
            m_repeatTimers.insert(key, timer);
            timer->start();
        }
    } else if (state == InputRedirection::KeyboardKeyReleased) {
        auto it = m_repeatTimers.find(key);
        if (it != m_repeatTimers.end()) {
            delete it.value();
            m_repeatTimers.erase(it);
        }
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->keyEvent(&event)) {
            return;
        }
    }
}

void KeyboardInputRedirection::processModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_inited) {
        return;
    }
    // TODO: send to proper Client and also send when active Client changes
    Qt::KeyboardModifiers oldMods = modifiers();
    m_xkb->updateModifiers(modsDepressed, modsLatched, modsLocked, group);
    if (oldMods != modifiers()) {
        emit m_input->keyboardModifiersChanged(modifiers(), oldMods);
    }
}

void KeyboardInputRedirection::processKeymapChange(int fd, uint32_t size)
{
    if (!m_inited) {
        return;
    }
    // TODO: should we pass the keymap to our Clients? Or only to the currently active one and update
    m_xkb->installKeymap(fd, size);
}

}

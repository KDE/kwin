/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xkb.h"
#include "xkb_qt_mapping.h"
#include "utils.h"
// frameworks
#include <KConfigGroup>
// KWayland
#include <KWaylandServer/seat_interface.h>
// Qt
#include <QTemporaryFile>
#include <QKeyEvent>
// xkbcommon
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>
// system
#include <sys/mman.h>
#include <unistd.h>
#include <bitset>

Q_LOGGING_CATEGORY(KWIN_XKB, "kwin_xkbcommon", QtCriticalMsg)

namespace KWin
{

static void xkbLogHandler(xkb_context *context, xkb_log_level priority, const char *format, va_list args)
{
    Q_UNUSED(context)
    char buf[1024];
    int length = std::vsnprintf(buf, 1023, format, args);
    while (length > 0 && std::isspace(buf[length - 1])) {
        --length;
    }
    if (length <= 0) {
        return;
    }
    switch (priority) {
    case XKB_LOG_LEVEL_DEBUG:
        qCDebug(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    case XKB_LOG_LEVEL_INFO:
        qCInfo(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    case XKB_LOG_LEVEL_WARNING:
        qCWarning(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    case XKB_LOG_LEVEL_ERROR:
    case XKB_LOG_LEVEL_CRITICAL:
    default:
        qCCritical(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    }
}

Xkb::Xkb(QObject *parent)
    : QObject(parent)
    , m_context(xkb_context_new(XKB_CONTEXT_NO_FLAGS))
    , m_keymap(nullptr)
    , m_state(nullptr)
    , m_shiftModifier(0)
    , m_capsModifier(0)
    , m_controlModifier(0)
    , m_altModifier(0)
    , m_metaModifier(0)
    , m_numModifier(0)
    , m_numLock(0)
    , m_capsLock(0)
    , m_scrollLock(0)
    , m_modifiers(Qt::NoModifier)
    , m_consumedModifiers(Qt::NoModifier)
    , m_keysym(XKB_KEY_NoSymbol)
    , m_leds()
{
    qRegisterMetaType<KWin::Xkb::LEDs>();
    if (!m_context) {
        qCDebug(KWIN_XKB) << "Could not create xkb context";
    } else {
        xkb_context_set_log_level(m_context, XKB_LOG_LEVEL_DEBUG);
        xkb_context_set_log_fn(m_context, &xkbLogHandler);

        // get locale as described in xkbcommon doc
        // cannot use QLocale as it drops the modifier part
        QByteArray locale = qgetenv("LC_ALL");
        if (locale.isEmpty()) {
            locale = qgetenv("LC_CTYPE");
        }
        if (locale.isEmpty()) {
            locale = qgetenv("LANG");
        }
        if (locale.isEmpty()) {
            locale = QByteArrayLiteral("C");
        }

        m_compose.table = xkb_compose_table_new_from_locale(m_context, locale.constData(), XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (m_compose.table) {
            m_compose.state = xkb_compose_state_new(m_compose.table, XKB_COMPOSE_STATE_NO_FLAGS);
        }
    }
}

Xkb::~Xkb()
{
    xkb_compose_state_unref(m_compose.state);
    xkb_compose_table_unref(m_compose.table);
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);
    xkb_context_unref(m_context);
}

void Xkb::reconfigure()
{
    if (!m_context) {
        return;
    }

    xkb_keymap *keymap = nullptr;
    if (!qEnvironmentVariableIsSet("KWIN_XKB_DEFAULT_KEYMAP")) {
        keymap = loadKeymapFromConfig();
    }
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

static bool stringIsEmptyOrNull(const char *str)
{
    return str == nullptr || str[0] == '\0';
}

/**
 * libxkbcommon uses secure_getenv to read the XKB_DEFAULT_* variables.
 * As kwin_wayland may have the CAP_SET_NICE capability, it returns nullptr
 * so we need to do it ourselves (see xkb_context_sanitize_rule_names).
**/
static void applyEnvironmentRules(xkb_rule_names &ruleNames)
{
    if (stringIsEmptyOrNull(ruleNames.rules)) {
        ruleNames.rules = getenv("XKB_DEFAULT_RULES");
    }

    if (stringIsEmptyOrNull(ruleNames.model)) {
        ruleNames.model = getenv("XKB_DEFAULT_MODEL");
    }

    if (stringIsEmptyOrNull(ruleNames.layout)) {
        ruleNames.layout = getenv("XKB_DEFAULT_LAYOUT");
        ruleNames.variant = getenv("XKB_DEFAULT_VARIANT");
    }

    if (ruleNames.options == nullptr) {
        ruleNames.options = getenv("XKB_DEFAULT_OPTIONS");
    }
}

xkb_keymap *Xkb::loadKeymapFromConfig()
{
    // load config
    if (!m_config) {
        return nullptr;
    }
    const KConfigGroup config = m_config->group("Layout");
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
    applyEnvironmentRules(ruleNames);
    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

xkb_keymap *Xkb::loadDefaultKeymap()
{
    xkb_rule_names ruleNames = {};
    applyEnvironmentRules(ruleNames);
    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

void Xkb::installKeymap(int fd, uint32_t size)
{
    if (!m_context) {
        return;
    }
    char *map = reinterpret_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
    if (map == MAP_FAILED) {
        return;
    }
    xkb_keymap *keymap = xkb_keymap_new_from_string(m_context, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_MAP_COMPILE_PLACEHOLDER);
    munmap(map, size);
    if (!keymap) {
        qCDebug(KWIN_XKB) << "Could not map keymap from file";
        return;
    }
    m_ownership = Ownership::Client;
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
    m_capsModifier    = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CAPS);
    m_controlModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CTRL);
    m_altModifier     = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_ALT);
    m_metaModifier    = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_LOGO);
    m_numModifier     = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_NUM);

    m_numLock         = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_NUM);
    m_capsLock        = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_CAPS);
    m_scrollLock      = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_SCROLL);

    m_currentLayout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);

    m_modifierState.depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    m_modifierState.latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));

    static bool s_startup = true;
    if (s_startup || qEnvironmentVariableIsSet("KWIN_FORCE_NUM_LOCK_EVALUATION")) {
        s_startup = false;
        if (m_ownership == Ownership::Server && m_numModifier != XKB_MOD_INVALID && m_numLockConfig) {
            const KConfigGroup config = m_numLockConfig->group("Keyboard");
            // STATE_ON = 0,  STATE_OFF = 1, STATE_UNCHANGED = 2, see plasma-desktop/kcms/keyboard/kcmmisc.h
            const auto setting = config.readEntry("NumLock", 2);
            const bool numLockIsOn = xkb_state_mod_index_is_active(m_state, m_numModifier, XKB_STATE_MODS_LOCKED);
            if ((setting == 0 && !numLockIsOn) || (setting == 1 && numLockIsOn)) {
                std::bitset<sizeof(xkb_mod_mask_t)*8> mask{m_modifierState.locked};
                if (mask.size() > m_numModifier) {
                    mask[m_numModifier] = (setting == 0);
                    m_modifierState.locked = mask.to_ulong();
                    xkb_state_update_mask(m_state, m_modifierState.depressed, m_modifierState.latched, m_modifierState.locked, 0, 0, m_currentLayout);
                    m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
                }
            }
        }
    }

    createKeymapFile();
    forwardModifiers();
    updateModifiers();
}

void Xkb::createKeymapFile()
{
    if (!m_seat) {
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

    QTemporaryFile *tmp = new QTemporaryFile(this);
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
    m_seat->setKeymap(tmp->handle(), size);
}

void Xkb::updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_mask(m_state, modsDepressed, modsLatched, modsLocked, 0, 0, group);
    updateModifiers();
    forwardModifiers();
}

void Xkb::updateKey(uint32_t key, InputRedirection::KeyboardKeyState state)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_key(m_state, key + 8, static_cast<xkb_key_direction>(state));
    if (state == InputRedirection::KeyboardKeyPressed) {
        const auto sym = toKeysym(key);
        if (m_compose.state && xkb_compose_state_feed(m_compose.state, sym) == XKB_COMPOSE_FEED_ACCEPTED) {
            switch (xkb_compose_state_get_status(m_compose.state)) {
            case XKB_COMPOSE_NOTHING:
                m_keysym = sym;
                break;
            case XKB_COMPOSE_COMPOSED:
                m_keysym = xkb_compose_state_get_one_sym(m_compose.state);
                break;
            default:
                m_keysym = XKB_KEY_NoSymbol;
                break;
            }
        } else {
            m_keysym = sym;
        }
    }
    updateModifiers();
    updateConsumedModifiers(key);
}

void Xkb::updateModifiers()
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_EFFECTIVE) == 1 ||
        xkb_state_mod_index_is_active(m_state, m_capsModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
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
    if (isKeypadKey(m_keysym)) {
        mods |= Qt::KeypadModifier;
    }
    m_modifiers = mods;

    // update LEDs
    LEDs leds;
    if (xkb_state_led_index_is_active(m_state, m_numLock) == 1) {
        leds = leds | LED::NumLock;
    }
    if (xkb_state_led_index_is_active(m_state, m_capsLock) == 1) {
        leds = leds | LED::CapsLock;
    }
    if (xkb_state_led_index_is_active(m_state, m_scrollLock) == 1) {
        leds = leds | LED::ScrollLock;
    }
    if (m_leds != leds) {
        m_leds = leds;
        emit ledsChanged(m_leds);
    }

    m_currentLayout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);
    m_modifierState.depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    m_modifierState.latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
}

void Xkb::forwardModifiers()
{
    if (!m_seat) {
        return;
    }
    m_seat->updateKeyboardModifiers(m_modifierState.depressed,
                                                     m_modifierState.latched,
                                                     m_modifierState.locked,
                                                     m_currentLayout);
}

QString Xkb::layoutName() const
{
    return layoutName(m_currentLayout);
}

QString Xkb::layoutName(xkb_layout_index_t layout) const
{
    if (!m_keymap) {
        return QString{};
    }
    return QString::fromLocal8Bit(xkb_keymap_layout_get_name(m_keymap, layout));
}

QMap<xkb_layout_index_t, QString> Xkb::layoutNames() const
{
    QMap<xkb_layout_index_t, QString> layouts;
    const auto size = m_keymap ? xkb_keymap_num_layouts(m_keymap) : 0u;
    for (xkb_layout_index_t i = 0; i < size; i++) {
        layouts.insert(i, layoutName(i));
    }
    return layouts;
}

void Xkb::updateConsumedModifiers(uint32_t key)
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_consumed2(m_state, key + 8, m_shiftModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + 8, m_altModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + 8, m_controlModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + 8, m_metaModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::MetaModifier;
    }
    m_consumedModifiers = mods;
}

Qt::KeyboardModifiers Xkb::modifiersRelevantForGlobalShortcuts() const
{
    if (!m_state) {
        return Qt::NoModifier;
    }
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

    Qt::KeyboardModifiers consumedMods = m_consumedModifiers;
    if ((mods & Qt::ShiftModifier) && (consumedMods == Qt::ShiftModifier)) {
        // test whether current keysym is a letter
        // in that case the shift should be removed from the consumed modifiers again
        // otherwise it would not be possible to trigger e.g. Shift+W as a shortcut
        // see BUG: 370341
        if (QChar(toQtKey(m_keysym)).isLetter()) {
            consumedMods = Qt::KeyboardModifiers();
        }
    }

    return mods & ~consumedMods;
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

Qt::Key Xkb::toQtKey(xkb_keysym_t keysym) const
{
    return xkbToQtKey(keysym);
}

xkb_keysym_t Xkb::fromQtKey(Qt::Key key, Qt::KeyboardModifiers mods) const
{
    return qtKeyToXkb(key, mods);
}

xkb_keysym_t Xkb::fromKeyEvent(QKeyEvent *event) const
{
    xkb_keysym_t sym = xkb_keysym_from_name(event->text().toUtf8().constData(), XKB_KEYSYM_NO_FLAGS);
    if (sym == XKB_KEY_NoSymbol) {
        // mapping from text failed, try mapping through KKeyServer
        sym = fromQtKey(Qt::Key(event->key() & ~Qt::KeyboardModifierMask), event->modifiers());
    }
    return sym;
}

bool Xkb::shouldKeyRepeat(quint32 key) const
{
    if (!m_keymap) {
        return false;
    }
    return xkb_keymap_key_repeats(m_keymap, key + 8) != 0;
}

void Xkb::switchToNextLayout()
{
    if (!m_keymap || !m_state) {
        return;
    }
    const xkb_layout_index_t numLayouts = xkb_keymap_num_layouts(m_keymap);
    const xkb_layout_index_t nextLayout = (xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE) + 1) % numLayouts;
    switchToLayout(nextLayout);
}

void Xkb::switchToPreviousLayout()
{
    if (!m_keymap || !m_state) {
        return;
    }
    const xkb_layout_index_t previousLayout = m_currentLayout == 0 ? numberOfLayouts() - 1 : m_currentLayout -1;
    switchToLayout(previousLayout);
}

void Xkb::switchToLayout(xkb_layout_index_t layout)
{
    if (!m_keymap || !m_state) {
        return;
    }
    if (layout >= numberOfLayouts()) {
        return;
    }
    const xkb_mod_mask_t depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    const xkb_mod_mask_t latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    const xkb_mod_mask_t locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
    xkb_state_update_mask(m_state, depressed, latched, locked, 0, 0, layout);
    updateModifiers();
    forwardModifiers();
}

quint32 Xkb::numberOfLayouts() const
{
    if (!m_keymap) {
        return 0;
    }
    return xkb_keymap_num_layouts(m_keymap);
}

void Xkb::setSeat(KWaylandServer::SeatInterface *seat)
{
    m_seat = QPointer<KWaylandServer::SeatInterface>(seat);
}

}

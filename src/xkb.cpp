/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xkb.h"
#include "dbusproperties_interface.h"
#include "inputmethod.h"
#include "utils/c_ptr.h"
#include "utils/common.h"
#include "wayland/inputmethod_v1_interface.h"
#include "wayland/keyboard_interface.h"
#include "wayland/seat_interface.h"
// frameworks
#include <KConfigGroup>
// Qt
#include <QKeyEvent>
#include <QTemporaryFile>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtXkbCommonSupport/private/qxkbcommon_p.h>
#else
#include <QtGui/private/qxkbcommon_p.h>
#endif
// xkbcommon
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>
// system
#include "main.h"
#include <bitset>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(KWIN_XKB, "kwin_xkbcommon", QtWarningMsg)

/* The offset between KEY_* numbering, and keycodes in the XKB evdev
 * dataset. */
static const int EVDEV_OFFSET = 8;
static const char *s_locale1Interface = "org.freedesktop.locale1";

namespace KWin
{

static void xkbLogHandler(xkb_context *context, xkb_log_level priority, const char *format, va_list args)
{
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

#if HAVE_XKBCOMMON_NO_SECURE_GETENV
constexpr xkb_context_flags KWIN_XKB_CONTEXT_FLAGS = XKB_CONTEXT_NO_SECURE_GETENV;
#else
constexpr xkb_context_flags KWIN_XKB_CONTEXT_FLAGS = XKB_CONTEXT_NO_FLAGS;
#endif

Xkb::Xkb(bool followLocale1)
    : m_context(xkb_context_new(KWIN_XKB_CONTEXT_FLAGS))
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
    , m_followLocale1(followLocale1)
{
    qRegisterMetaType<KWin::LEDs>();
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

    if (m_followLocale1) {
        bool connected = QDBusConnection::systemBus().connect(s_locale1Interface, "/org/freedesktop/locale1", QStringLiteral("org.freedesktop.DBus.Properties"),
                                                              QStringLiteral("PropertiesChanged"),
                                                              this,
                                                              SLOT(reconfigure()));
        if (!connected) {
            qCWarning(KWIN_XKB) << "Could not connect to org.freedesktop.locale1";
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

void Xkb::setConfig(const KSharedConfigPtr &config)
{
    m_configGroup = config->group("Layout");
}

void Xkb::setNumLockConfig(const KSharedConfigPtr &config)
{
    m_numLockConfig = config;
}

void Xkb::reconfigure()
{
    if (!m_context) {
        return;
    }

    xkb_keymap *keymap = nullptr;
    if (!qEnvironmentVariableIsSet("KWIN_XKB_DEFAULT_KEYMAP")) {
        if (m_followLocale1) {
            keymap = loadKeymapFromLocale1();
        } else {
            keymap = loadKeymapFromConfig();
        }
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
void Xkb::applyEnvironmentRules(xkb_rule_names &ruleNames)
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
    if (!m_configGroup.isValid()) {
        return nullptr;
    }
    const QByteArray model = m_configGroup.readEntry("Model", "pc104").toLatin1();
    const QByteArray layout = m_configGroup.readEntry("LayoutList").toLatin1();
    const QByteArray variant = m_configGroup.readEntry("VariantList").toLatin1();
    const QByteArray options = m_configGroup.readEntry("Options").toLatin1();

    xkb_rule_names ruleNames = {
        .rules = nullptr,
        .model = model.constData(),
        .layout = layout.constData(),
        .variant = variant.constData(),
        .options = nullptr,
    };

    if (m_configGroup.readEntry("ResetOldOptions", false)) {
        ruleNames.options = options.constData();
    }

    applyEnvironmentRules(ruleNames);

    m_layoutList = QString::fromLatin1(ruleNames.layout).split(QLatin1Char(','));

    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

xkb_keymap *Xkb::loadDefaultKeymap()
{
    xkb_rule_names ruleNames = {};
    applyEnvironmentRules(ruleNames);
    m_layoutList = QString::fromLatin1(ruleNames.layout).split(QLatin1Char(','));
    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

xkb_keymap *Xkb::loadKeymapFromLocale1()
{
    OrgFreedesktopDBusPropertiesInterface locale1Properties(s_locale1Interface, "/org/freedesktop/locale1", QDBusConnection::systemBus(), this);
    const QVariantMap properties = locale1Properties.GetAll(s_locale1Interface);

    const QByteArray model = properties["X11Model"].toByteArray();
    const QByteArray layout = properties["X11Layout"].toByteArray();
    const QByteArray variant = properties["X11Variant"].toByteArray();
    const QByteArray options = properties["X11Options"].toByteArray();

    xkb_rule_names ruleNames = {
        .rules = nullptr,
        .model = model.constData(),
        .layout = layout.constData(),
        .variant = variant.constData(),
        .options = options.constData(),
    };

    applyEnvironmentRules(ruleNames);

    m_layoutList = QString::fromLatin1(ruleNames.layout).split(QLatin1Char(','));

    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
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

    // save Locks
    bool numLockIsOn, capsLockIsOn;
    static bool s_startup = true;
    if (!s_startup) {
        numLockIsOn = xkb_state_mod_index_is_active(m_state, m_numModifier, XKB_STATE_MODS_LOCKED);
        capsLockIsOn = xkb_state_mod_index_is_active(m_state, m_capsModifier, XKB_STATE_MODS_LOCKED);
    }

    // now release the old ones
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);

    m_keymap = keymap;
    m_state = state;

    m_shiftModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_SHIFT);
    m_capsModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CAPS);
    m_controlModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CTRL);
    m_altModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_ALT);
    m_metaModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_LOGO);
    m_numModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_NUM);

    m_numLock = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_NUM);
    m_capsLock = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_CAPS);
    m_scrollLock = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_SCROLL);

    m_currentLayout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);

    m_modifierState.depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    m_modifierState.latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));

    auto setLock = [this](xkb_mod_index_t modifier, bool value) {
        if (modifier != XKB_MOD_INVALID) {
            std::bitset<sizeof(xkb_mod_mask_t) * 8> mask{m_modifierState.locked};
            if (mask.size() > modifier) {
                mask[modifier] = value;
                m_modifierState.locked = mask.to_ulong();
                xkb_state_update_mask(m_state, m_modifierState.depressed, m_modifierState.latched, m_modifierState.locked, 0, 0, m_currentLayout);
                m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
            }
        }
    };

    if (s_startup || qEnvironmentVariableIsSet("KWIN_FORCE_NUM_LOCK_EVALUATION")) {
        s_startup = false;
        if (m_numLockConfig) {
            const KConfigGroup config = m_numLockConfig->group("Keyboard");
            // STATE_ON = 0,  STATE_OFF = 1, STATE_UNCHANGED = 2, see plasma-desktop/kcms/keyboard/kcmmisc.h
            const auto setting = config.readEntry("NumLock", 2);
            if (setting != 2) {
                setLock(m_numModifier, !setting);
            }
        }
    } else {
        setLock(m_numModifier, numLockIsOn);
        setLock(m_capsModifier, capsLockIsOn);
    }

    createKeymapFile();
    forwardModifiers();
    if (auto *inputmethod = kwinApp()->inputMethod()) {
        inputmethod->forwardModifiers(InputMethod::Force);
    }
    updateModifiers();
}

void Xkb::createKeymapFile()
{
    const auto currentKeymap = keymapContents();
    if (currentKeymap.isEmpty()) {
        return;
    }
    m_seat->keyboard()->setKeymap(currentKeymap);
    auto *inputmethod = kwinApp()->inputMethod();
    if (!inputmethod) {
        return;
    }
    if (auto *keyboardGrab = inputmethod->keyboardGrab()) {
        keyboardGrab->sendKeymap(currentKeymap);
    }
}

QByteArray Xkb::keymapContents() const
{
    if (!m_seat || !m_seat->keyboard()) {
        return {};
    }
    // TODO: uninstall keymap on server?
    if (!m_keymap) {
        return {};
    }

    UniqueCPtr<char> keymapString(xkb_keymap_get_as_string(m_keymap, XKB_KEYMAP_FORMAT_TEXT_V1));
    if (!keymapString) {
        return {};
    }
    return keymapString.get();
}

void Xkb::updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_keymap || !m_state) {
        return;
    }
    // Avoid to create a infinite loop between input method and compositor.
    if (xkb_state_update_mask(m_state, modsDepressed, modsLatched, modsLocked, 0, 0, group) == 0) {
        return;
    }
    updateModifiers();
    forwardModifiers();
}

void Xkb::updateKey(uint32_t key, InputRedirection::KeyboardKeyState state)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_key(m_state, key + EVDEV_OFFSET, static_cast<xkb_key_direction>(state));
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
    if (m_keysym >= XKB_KEY_KP_Space && m_keysym <= XKB_KEY_KP_9) {
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
        Q_EMIT ledsChanged(m_leds);
    }

    const uint32_t newLayout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);
    const uint32_t depressed = xkb_state_serialize_mods(m_state, XKB_STATE_MODS_DEPRESSED);
    const uint32_t latched = xkb_state_serialize_mods(m_state, XKB_STATE_MODS_LATCHED);
    const uint32_t locked = xkb_state_serialize_mods(m_state, XKB_STATE_MODS_LOCKED);

    if (newLayout != m_currentLayout || depressed != m_modifierState.depressed || latched != m_modifierState.latched || locked != m_modifierState.locked) {
        m_currentLayout = newLayout;
        m_modifierState.depressed = depressed;
        m_modifierState.latched = latched;
        m_modifierState.locked = locked;

        Q_EMIT modifierStateChanged();
    }
}

void Xkb::forwardModifiers()
{
    if (!m_seat || !m_seat->keyboard()) {
        return;
    }
    m_seat->notifyKeyboardModifiers(m_modifierState.depressed,
                                    m_modifierState.latched,
                                    m_modifierState.locked,
                                    m_currentLayout);
}

QString Xkb::layoutName(xkb_layout_index_t index) const
{
    if (!m_keymap) {
        return QString{};
    }
    return QString::fromLocal8Bit(xkb_keymap_layout_get_name(m_keymap, index));
}

QString Xkb::layoutName() const
{
    return layoutName(m_currentLayout);
}

QString Xkb::layoutShortName(int index) const
{
    return m_layoutList.value(index);
}

void Xkb::updateConsumedModifiers(uint32_t key)
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_shiftModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_altModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_controlModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_metaModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::MetaModifier;
    }
    m_consumedModifiers = mods;
}

Qt::KeyboardModifiers Xkb::modifiersRelevantForGlobalShortcuts(uint32_t scanCode) const
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
        if (QChar::isLetter(toQtKey(m_keysym, scanCode, Qt::ControlModifier))) {
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

    // Workaround because there's some kind of overlap between KEY_ZENKAKUHANKAKU and TLDE
    // This key is important because some hardware manufacturers use it to indicate touchpad toggling.
    xkb_keysym_t ret = xkb_state_key_get_one_sym(m_state, key + EVDEV_OFFSET);
    if (ret == 0 && key == KEY_ZENKAKUHANKAKU) {
        ret = XKB_KEY_Zenkaku_Hankaku;
    }
    return ret;
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

Qt::Key Xkb::toQtKey(xkb_keysym_t keySym,
                     uint32_t scanCode,
                     Qt::KeyboardModifiers modifiers,
                     bool superAsMeta) const
{
    // FIXME: passing superAsMeta doesn't have impact due to bug in the Qt function, so handle it below
    Qt::Key qtKey = Qt::Key(QXkbCommon::keysymToQtKey(keySym, modifiers, m_state, scanCode + EVDEV_OFFSET, superAsMeta));

    // FIXME: workarounds for symbols currently wrong/not mappable via keysymToQtKey()
    if (superAsMeta && (qtKey == Qt::Key_Super_L || qtKey == Qt::Key_Super_R)) {
        // translate Super/Hyper keys to Meta if we're using them as the MetaModifier
        qtKey = Qt::Key_Meta;
    } else if (qtKey > 0xff && keySym <= 0xff) {
        // XKB_KEY_mu, XKB_KEY_ydiaeresis go here
        qtKey = Qt::Key(keySym);
#if QT_VERSION_MAJOR < 6 // since Qt 5 LTS is frozen
    } else if (keySym == XKB_KEY_Sys_Req) {
        // fixed in QTBUG-92087
        qtKey = Qt::Key_SysReq;
#endif
    }
    return qtKey;
}

bool Xkb::shouldKeyRepeat(quint32 key) const
{
    if (!m_keymap) {
        return false;
    }
    return xkb_keymap_key_repeats(m_keymap, key + EVDEV_OFFSET) != 0;
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
    const xkb_layout_index_t previousLayout = m_currentLayout == 0 ? numberOfLayouts() - 1 : m_currentLayout - 1;
    switchToLayout(previousLayout);
}

bool Xkb::switchToLayout(xkb_layout_index_t layout)
{
    if (!m_keymap || !m_state || layout >= numberOfLayouts()) {
        return false;
    }
    const xkb_mod_mask_t depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    const xkb_mod_mask_t latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    const xkb_mod_mask_t locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
    xkb_state_update_mask(m_state, depressed, latched, locked, 0, 0, layout);
    updateModifiers();
    forwardModifiers();
    return true;
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

std::optional<int> Xkb::keycodeFromKeysym(xkb_keysym_t keysym)
{
    auto layout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);
    const xkb_keycode_t max = xkb_keymap_max_keycode(m_keymap);
    for (xkb_keycode_t keycode = xkb_keymap_min_keycode(m_keymap); keycode < max; keycode++) {
        uint levelCount = xkb_keymap_num_levels_for_key(m_keymap, keycode, layout);
        for (uint currentLevel = 0; currentLevel < levelCount; currentLevel++) {
            const xkb_keysym_t *syms;
            uint num_syms = xkb_keymap_key_get_syms_by_level(m_keymap, keycode, layout, currentLevel, &syms);
            for (uint sym = 0; sym < num_syms; sym++) {
                if (syms[sym] == keysym) {
                    return {keycode - EVDEV_OFFSET};
                }
            }
        }
    }
    return {};
}
}

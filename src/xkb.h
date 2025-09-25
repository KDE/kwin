/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input.h"
#include <xkbcommon/xkbcommon.h>

#include <kwin_export.h>

#include <KConfigGroup>

#include <QLoggingCategory>

#include <optional>

Q_DECLARE_LOGGING_CATEGORY(KWIN_XKB)

struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xkb_compose_table;
struct xkb_compose_state;
typedef uint32_t xkb_mod_index_t;
typedef uint32_t xkb_led_index_t;
typedef uint32_t xkb_keysym_t;
typedef uint32_t xkb_layout_index_t;

namespace KWin
{

class SeatInterface;

class KWIN_EXPORT Xkb : public QObject
{
    Q_OBJECT
public:
    enum Modifier {
        NoModifier = 0,
        Shift = 1 << 0,
        Lock = 1 << 1,
        Control = 1 << 2,
        Mod1 = 1 << 3,
        Num = 1 << 4,
        Mod3 = 1 << 5,
        Mod4 = 1 << 6,
        Mod5 = 1 << 7,
    };
    Q_ENUM(Modifier)
    Q_DECLARE_FLAGS(Modifiers, Modifier)

    Xkb(bool followLocale1 = false);
    ~Xkb() override;
    void setConfig(const KSharedConfigPtr &config);
    void setNumLockConfig(const KSharedConfigPtr &config);

    void updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    void updateKey(uint32_t key, KeyboardKeyState state);
    xkb_keysym_t toKeysym(uint32_t key);
    xkb_keysym_t currentKeysym() const
    {
        return m_keysym;
    }
    QString toString(xkb_keysym_t keysym);
    Qt::Key toQtKey(xkb_keysym_t keysym,
                    uint32_t scanCode = 0,
                    Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers()) const;
    Qt::KeyboardModifiers modifiers() const;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts(uint32_t scanCode = 0) const;
    bool shouldKeyRepeat(quint32 key) const;

    void switchToNextLayout();
    void switchToPreviousLayout();
    bool switchToLayout(xkb_layout_index_t layout);

    void setModifierLatched(KWin::Xkb::Modifier mod, bool latched);
    void setModifierLocked(KWin::Xkb::Modifier mod, bool locked);

    Modifiers depressedModifiers() const;
    Modifiers latchedModifiers() const;
    Modifiers lockedModifiers() const;

    LEDs leds() const
    {
        return m_leds;
    }

    xkb_keymap *keymap() const
    {
        return m_keymap;
    }

    xkb_state *state() const
    {
        return m_state;
    }

    quint32 currentLayout() const
    {
        return m_currentLayout;
    }

    const auto &modifierState() const
    {
        return m_modifierState;
    }
    QString layoutName(xkb_layout_index_t index) const;
    QString layoutName() const;
    QString layoutShortName(int index) const;
    quint32 numberOfLayouts() const;

    /**
     * Forwards the current modifier state to the Wayland seat
     */
    void forwardModifiers();

    void setSeat(SeatInterface *seat);
    QByteArray keymapContents() const;

    /**
     * Returns a pair of <keycode, level> for the given keysym.
     */
    struct KeyCode {
        xkb_keycode_t keyCode = 0;
        uint level = 0;
        xkb_mod_mask_t modifiers = 0;
    };
    std::optional<KeyCode> keycodeFromKeysym(xkb_keysym_t keysym);

    /**
     * Returns list of candidate keysyms corresponding to the given Qt key.
     *
     * Internally filters the results based on whether keyQt has the numlock modifier.
     */
    static QList<xkb_keysym_t> keysymsFromQtKey(QKeyCombination keyQt);

    // Create a new keymap with one custom keysym bound to a given keycode.
    QByteArray createKeymapForKeysym(xkb_keycode_t newKeycode,
                                     xkb_keysym_t customSym);

public Q_SLOTS:
    void reconfigure();

Q_SIGNALS:
    void ledsChanged(const LEDs &leds);
    void modifierStateChanged();

private:
    void applyEnvironmentRules(xkb_rule_names &);
    xkb_keymap *loadKeymapFromConfig();
    xkb_keymap *loadDefaultKeymap();
    xkb_keymap *loadKeymapFromLocale1();
    void updateKeymap(xkb_keymap *keymap);
    void createKeymapFile();
    void updateModifiers();
    void updateConsumedModifiers(uint32_t key);
    xkb_context *m_context;
    xkb_keymap *m_keymap;
    QStringList m_layoutList;
    QStringList m_variantList;
    xkb_state *m_state;
    xkb_mod_index_t m_shiftModifier;
    xkb_mod_index_t m_capsModifier;
    xkb_mod_index_t m_controlModifier;
    xkb_mod_index_t m_altModifier;
    xkb_mod_index_t m_metaModifier;
    xkb_mod_index_t m_numModifier;
    xkb_mod_index_t m_mod5Modifier;
    xkb_led_index_t m_numLock;
    xkb_led_index_t m_capsLock;
    xkb_led_index_t m_scrollLock;
    xkb_led_index_t m_composeLed;
    xkb_led_index_t m_kanaLed;
    Qt::KeyboardModifiers m_modifiers;
    Qt::KeyboardModifiers m_consumedModifiers;
    xkb_keysym_t m_keysym;
    quint32 m_currentLayout = 0;

    struct
    {
        xkb_compose_table *table = nullptr;
        xkb_compose_state *state = nullptr;
    } m_compose;
    LEDs m_leds;
    KConfigGroup m_configGroup;
    KSharedConfigPtr m_numLockConfig;

    struct ModifierState
    {
        xkb_mod_index_t depressed = 0;
        xkb_mod_index_t latched = 0;
        xkb_mod_index_t locked = 0;
    } m_modifierState;

    QPointer<SeatInterface> m_seat;
    const bool m_followLocale1;
};

inline Qt::KeyboardModifiers Xkb::modifiers() const
{
    return m_modifiers;
}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Xkb::Modifiers)

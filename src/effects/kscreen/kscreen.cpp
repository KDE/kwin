/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "kscreen.h"
// KConfigSkeleton
#include "kscreenconfig.h"
#include <QDebug>

/**
 * How this effect works:
 *
 * Effect announces that it is around through property _KDE_KWIN_KSCREEN_SUPPORT on the root window.
 *
 * KScreen watches for this property and when it wants to adjust screens, KScreen goes
 * through the following protocol:
 * 1. KScreen sets the property value to 1
 * 2. Effect starts to fade out all windows
 * 3. When faded out the effect sets property value to 2
 * 4. KScreen adjusts the screens
 * 5. KScreen sets property value to 3
 * 6. Effect starts to fade in all windows again
 * 7. Effect sets back property value to 0
 *
 * The property has type 32 bits cardinal. To test it use:
 * xprop -root -f _KDE_KWIN_KSCREEN_SUPPORT 32c -set _KDE_KWIN_KSCREEN_SUPPORT 1
 *
 * The states are:
 * 0: normal
 * 1: fading out
 * 2: faded out
 * 3: fading in
 */

namespace KWin
{

KscreenEffect::KscreenEffect()
    : Effect()
    , m_lastPresentTime(std::chrono::milliseconds::zero())
    , m_state(StateNormal)
    , m_atom(effects->announceSupportProperty("_KDE_KWIN_KSCREEN_SUPPORT", this))
{
    initConfig<KscreenConfig>();
    connect(effects, &EffectsHandler::propertyNotify, this, &KscreenEffect::propertyNotify);
    connect(effects, &EffectsHandler::xcbConnectionChanged, this,
        [this] {
            m_atom = effects->announceSupportProperty(QByteArrayLiteral("_KDE_KWIN_KSCREEN_SUPPORT"), this);
        }
    );
    reconfigure(ReconfigureAll);

    const QList<EffectScreen *> screens = effects->screens();
    for (auto screen : screens) {
        addScreen(screen);
    }
    connect(effects, &EffectsHandler::screenAdded, this, &KscreenEffect::addScreen);
}

KscreenEffect::~KscreenEffect()
{
}

void KscreenEffect::addScreen(EffectScreen *screen)
{
    connect(screen, &EffectScreen::wakeUp, this, [this] {
        setState(StateFadingIn);
    });
    connect(screen, &EffectScreen::aboutToTurnOff, this, [this] (std::chrono::milliseconds dimmingIn) {
        m_timeLine.setDuration(dimmingIn);
        setState(StateFadingOut);
    });
}

void KscreenEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    KscreenConfig::self()->read();
    m_timeLine.setDuration(
        std::chrono::milliseconds(animationTime<KscreenConfig>(250)));
}

void KscreenEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    std::chrono::milliseconds delta = std::chrono::milliseconds::zero();
    if (m_lastPresentTime.count()) {
        delta = presentTime - m_lastPresentTime;
    }

    if (m_state == StateFadingIn || m_state == StateFadingOut) {
        m_timeLine.update(delta);
        if (m_timeLine.done()) {
            switchState();
        }
    }

    if (isActive()) {
        m_lastPresentTime = presentTime;
    } else {
        m_lastPresentTime = std::chrono::milliseconds::zero();
    }

    effects->prePaintScreen(data, presentTime);
}

void KscreenEffect::postPaintScreen()
{
    if (m_state == StateFadingIn || m_state == StateFadingOut) {
        effects->addRepaintFull();
    }
}

void KscreenEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_state != StateNormal) {
        data.setTranslucent();
    }
    effects->prePaintWindow(w, data, presentTime);
}

void KscreenEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    //fade to black and fully opaque
    switch (m_state) {
        case StateFadingOut:
            data.setOpacity(data.opacity() + (1.0 - data.opacity()) * m_timeLine.value());
            data.multiplyBrightness(1.0 - m_timeLine.value());
            break;
        case StateFadedOut:
            data.multiplyOpacity(0.0);
            data.multiplyBrightness(0.0);
            break;
        case StateFadingIn:
            data.setOpacity(data.opacity() + (1.0 - data.opacity()) * (1.0 - m_timeLine.value()));
            data.multiplyBrightness(m_timeLine.value());
            break;
        default:
            // no adjustment
            break;
    }
    effects->paintWindow(w, mask, region, data);
}

void KscreenEffect::setState(FadeOutState state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    m_timeLine.reset();
    m_lastPresentTime = std::chrono::milliseconds::zero();
    effects->addRepaintFull();
}

void KscreenEffect::propertyNotify(EffectWindow *window, long int atom)
{
    if (window || atom != m_atom || m_atom == XCB_ATOM_NONE) {
        return;
    }
    const QByteArray byteData = effects->readRootProperty(m_atom, XCB_ATOM_CARDINAL, 32);
    const uint32_t *data = byteData.isEmpty() ? nullptr : reinterpret_cast<const uint32_t *>(byteData.data());
    if (!data || data[0] >= LastState) { // Property was deleted
        if (data) {
            qCDebug(KWINEFFECTS) << "Incorrect Property state, immediate stop: " << data[0];
        }
        setState(StateNormal);
        return;
    }

    setState(FadeOutState(data[0]));
}

void KscreenEffect::switchState()
{
    long value = -1l;
    if (m_state == StateFadingOut) {
        m_state = StateFadedOut;
        value = 2l;
    } else if (m_state == StateFadingIn) {
        m_state = StateNormal;
        value = 0l;
    }
    if (value != -1l && m_atom != XCB_ATOM_NONE) {
        xcb_change_property(xcbConnection(), XCB_PROP_MODE_REPLACE, x11RootWindow(), m_atom, XCB_ATOM_CARDINAL, 32, 1, &value);
    }
}

bool KscreenEffect::isActive() const
{
    return m_state != StateNormal;
}

} // namespace KWin

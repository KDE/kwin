/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
// own
#include "kscreen.h"
// KConfigSkeleton
#include "kscreenconfig.h"

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
 **/

namespace KWin
{

KscreenEffect::KscreenEffect()
    : Effect()
    , m_state(StateNormal)
    , m_atom(effects->announceSupportProperty("_KDE_KWIN_KSCREEN_SUPPORT", this))
{
    initConfig<KscreenConfig>();
    connect(effects, SIGNAL(propertyNotify(KWin::EffectWindow*,long)), SLOT(propertyNotify(KWin::EffectWindow*,long)));
    connect(effects, &EffectsHandler::xcbConnectionChanged, this,
        [this] {
            m_atom = effects->announceSupportProperty(QByteArrayLiteral("_KDE_KWIN_KSCREEN_SUPPORT"), this);
        }
    );
    reconfigure(ReconfigureAll);
}

KscreenEffect::~KscreenEffect()
{
}

void KscreenEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    KscreenConfig::self()->read();
    m_timeLine.setDuration(animationTime<KscreenConfig>(250));
}

void KscreenEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    if (m_state == StateFadingIn || m_state == StateFadingOut) {
        m_timeLine.setCurrentTime(m_timeLine.currentTime() + time);
        if (m_timeLine.currentValue() >= 1.0) {
            switchState();
        }
    }
    effects->prePaintScreen(data, time);
}

void KscreenEffect::postPaintScreen()
{
    if (m_state == StateFadingIn || m_state == StateFadingOut) {
        effects->addRepaintFull();
    }
}

void KscreenEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    if (m_state != StateNormal) {
        data.setTranslucent();
    }
    effects->prePaintWindow(w, data, time);
}

void KscreenEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    switch (m_state) {
    case StateFadingOut:
        data.multiplyOpacity(1.0 - m_timeLine.currentValue());
        break;
    case StateFadedOut:
        data.multiplyOpacity(0.0);
        break;
    case StateFadingIn:
        data.multiplyOpacity(m_timeLine.currentValue());
        break;
    default:
        // no adjustment
        break;
    }
    effects->paintWindow(w, mask, region, data);
}

void KscreenEffect::propertyNotify(EffectWindow *window, long int atom)
{
    if (window || atom != m_atom || m_atom == XCB_ATOM_NONE) {
        return;
    }
    QByteArray byteData = effects->readRootProperty(m_atom, XCB_ATOM_CARDINAL, 32);
    const uint32_t *data = byteData.isEmpty() ? nullptr : reinterpret_cast<const uint32_t *>(byteData.data());
    if (!data // Property was deleted
        || data[0] == 0) { // normal state - KWin should have switched to it
        if (m_state != StateNormal) {
            m_state = StateNormal;
            effects->addRepaintFull();
        }
        return;
    }
    if (data[0] == 2) {
        // faded out state - KWin should have switched to it
        if (m_state != StateFadedOut) {
            m_state = StateFadedOut;
            effects->addRepaintFull();
        }
        return;
    }
    if (data[0] == 1) {
        // kscreen wants KWin to fade out all windows
        m_state = StateFadingOut;
        m_timeLine.setCurrentTime(0);
        effects->addRepaintFull();
        return;
    }
    if (data[0] == 3) {
        // kscreen wants KWin to fade in again
        m_state = StateFadingIn;
        m_timeLine.setCurrentTime(0);
        effects->addRepaintFull();
        return;
    }
    qCDebug(KWINEFFECTS) << "Incorrect Property state, immediate stop: " << data[0];
    m_state = StateNormal;
    effects->addRepaintFull();
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

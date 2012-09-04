/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "translucency.h"

#include <kconfiggroup.h>

namespace KWin
{

KWIN_EFFECT(translucency, TranslucencyEffect)

TranslucencyEffect::TranslucencyEffect()
    : fadeout(NULL)
    , current(NULL)
    , previous(NULL)
    , m_activeDecorations(false)
    , m_activeMoveResize(false)
    , m_activeDialogs(false)
    , m_activeInactive(false)
    , m_activeCombobox(false)
    , m_activeMenus(false)
    , m_active(false)
{
    reconfigure(ReconfigureAll);
    active = effects->activeWindow();
    connect(effects, SIGNAL(windowActivated(KWin::EffectWindow*)), this, SLOT(slotWindowActivated(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(checkIsActive()));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(checkIsActive()));
    connect(effects, SIGNAL(windowStartUserMovedResized(KWin::EffectWindow*)), this, SLOT(slotWindowStartStopUserMovedResized(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowFinishUserMovedResized(KWin::EffectWindow*)), this, SLOT(slotWindowStartStopUserMovedResized(KWin::EffectWindow*)));
}

void TranslucencyEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("Translucency");
    decoration = conf.readEntry("Decoration", 1.0);
    moveresize = conf.readEntry("MoveResize", 0.8);
    dialogs = conf.readEntry("Dialogs", 1.0);
    inactive = conf.readEntry("Inactive", 1.0);
    comboboxpopups = conf.readEntry("ComboboxPopups", 1.0);
    menus = conf.readEntry("Menus", 1.0);
    individualmenuconfig = conf.readEntry("IndividualMenuConfig", false);
    if (individualmenuconfig) {
        dropdownmenus = conf.readEntry("DropdownMenus", 1.0);
        popupmenus = conf.readEntry("PopupMenus", 1.0);
        tornoffmenus = conf.readEntry("TornOffMenus", 1.0);
    } else {
        dropdownmenus = menus;
        popupmenus = menus;
        tornoffmenus = menus;
    }
    moveresize_timeline.setCurveShape(QTimeLine::EaseInOutCurve);
    moveresize_timeline.setDuration(animationTime(conf, "Duration", 800));
    activeinactive_timeline.setCurveShape(QTimeLine::EaseInOutCurve);
    activeinactive_timeline.setDuration(animationTime(conf, "Duration", 800));

    m_activeDecorations = !qFuzzyCompare(decoration, 1.0);
    m_activeMoveResize = !qFuzzyCompare(moveresize, 1.0);
    m_activeDialogs = !qFuzzyCompare(dialogs, 1.0);
    m_activeInactive = !qFuzzyCompare(inactive, 1.0);
    m_activeCombobox = !qFuzzyCompare(comboboxpopups, 1.0);
    m_activeMenus = !qFuzzyCompare(menus, 1.0);
    if (!m_activeMenus && individualmenuconfig) {
        m_activeMenus = !qFuzzyCompare(dropdownmenus, 1.0) ||
                        !qFuzzyCompare(popupmenus, 1.0) ||
                        !qFuzzyCompare(tornoffmenus, 1.0);
    }
    checkIsActive();

    // Repaint the screen just in case the user changed the inactive opacity
    effects->addRepaintFull();
}

void TranslucencyEffect::checkIsActive()
{
    m_active = m_activeDecorations ||
               m_activeMoveResize ||
               m_activeDialogs ||
               m_activeInactive ||
               m_activeCombobox ||
               m_activeMenus;
    if (!m_active) {
        // all individual options are disabled, no window state can activate it
        return;
    }
    if (m_activeDecorations) {
        // we can assume that there is at least one decorated window, so the effect is active
        return;
    }
    if (m_activeInactive) {
        // we can assume that there is at least one inactive window, so the effect is active
        // TODO: maybe only if inactive window is not obscured?
        return;
    }
    // for all other options we go through the list of window and search for a Window being affected
    bool activeDropdown, activePopup, activeTornoff;
    activeDropdown = activePopup = activeTornoff = false;
    if (individualmenuconfig) {
        activeDropdown = !qFuzzyCompare(dropdownmenus, 1.0);
        activePopup = !qFuzzyCompare(popupmenus, 1.0);
        activeTornoff = !qFuzzyCompare(activeTornoff, 1.0);
    }
    foreach (EffectWindow *w, effects->stackingOrder()) {
        if (w->isDeleted()) {
            // ignore deleted windows
            continue;
        }
        if (m_activeMoveResize && (w->isUserMove() || w->isUserResize())) {
            return;
        }
        if (m_activeDialogs && w->isDialog()) {
            return;
        }
        if (m_activeCombobox && w->isComboBox()) {
            return;
        }
        if (m_activeMenus) {
            if (individualmenuconfig) {
                if (activeDropdown && w->isDropdownMenu()) {
                    return;
                }
                if (activePopup && w->isPopupMenu()) {
                    return;
                }
                if (activeTornoff && w->isMenu()) {
                    return;
                }
            } else {
                if (w->isMenu() || w->isDropdownMenu() || w->isPopupMenu()) {
                    return;
                }
            }
        }
    }
    // no matching window, disable effect
    m_active = false;
}

void TranslucencyEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    // We keep track of the windows that was last active so we know
    // which one to fade out and which ones to paint as fully inactive
    if (w == active && w != current) {
        previous = current;
        current = w;
    }

    moveresize_timeline.setCurrentTime(moveresize_timeline.currentTime() + time);
    activeinactive_timeline.setCurrentTime(activeinactive_timeline.currentTime() + time);

    if (decoration != 1.0 && w->hasDecoration()) {
        data.mask |= PAINT_WINDOW_TRANSLUCENT;
        // don't clear PAINT_WINDOW_OPAQUE, contents are not affected
        data.clip &= w->contentsRect().translated(w->pos());  // decoration cannot clip
    }
    if (inactive != 1.0 && (isInactive(w) || activeinactive_timeline.currentValue() < 1.0))
        data.setTranslucent();
    else if (moveresize != 1.0 && (w->isUserMove() || w->isUserResize() || w == fadeout)) {
        data.setTranslucent();
    }
    else if (dialogs != 1.0 && w->isDialog()) {
        data.setTranslucent();
    }
    else if ((dropdownmenus != 1.0 && w->isDropdownMenu())
            || (popupmenus != 1.0 && w->isPopupMenu())
            || (tornoffmenus != 1.0 && w->isMenu())
            || (comboboxpopups != 1.0 && w->isComboBox())) {
        data.setTranslucent();
    }

    effects->prePaintWindow(w, data, time);
}

void TranslucencyEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (w->isDesktop() || w->isDock()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }
    // Handling active and inactive windows
    if (inactive != 1.0 && isInactive(w)) {
        data.opacity *= inactive;

        if (w == previous) {
            data.opacity *= (inactive + ((1.0 - inactive) * (1.0 - activeinactive_timeline.currentValue())));
            if (activeinactive_timeline.currentValue() < 1.0)
                w->addRepaintFull();
            else
                previous = NULL;
        }
    } else {
        // Fading in
        if (!isInactive(w) && !w->isDesktop()) {
            data.opacity *= (inactive + ((1.0 - inactive) * activeinactive_timeline.currentValue()));
            if (activeinactive_timeline.currentValue() < 1.0)
                w->addRepaintFull();
        }
        // decoration and dialogs
        if (decoration != 1.0 && w->hasDecoration())
            data.decoration_opacity *= decoration;
        if (dialogs != 1.0 && w->isDialog())
            data.opacity *= dialogs;

        // Handling moving and resizing
        if (moveresize != 1.0 && !w->isDesktop() && !w->isDock()) {
            double progress = moveresize_timeline.currentValue();
            if (w->isUserMove() || w->isUserResize()) {
                // Fading to translucent
                data.opacity *= (moveresize + ((1.0 - moveresize) * (1.0 - progress)));
                if (progress < 1.0 && progress > 0.0) {
                    w->addRepaintFull();
                    fadeout = w;
                }
            } else {
                // Fading back to more opaque
                if (w == fadeout && !w->isUserMove() && !w->isUserResize()) {
                    data.opacity *= (moveresize + ((1.0 - moveresize) * (progress)));
                    if (progress == 1.0 || progress == 0.0)
                        fadeout = NULL;
                    else
                        w->addRepaintFull();

                }
            }
        }

        // Menus and combos
        if (dropdownmenus != 1.0 && w->isDropdownMenu())
            data.opacity *= dropdownmenus;
        if (popupmenus != 1.0 && w->isPopupMenu())
            data.opacity *= popupmenus;
        if (tornoffmenus != 1.0 && w->isMenu())
            data.opacity *= tornoffmenus;
        if (comboboxpopups != 1.0 && w->isComboBox())
            data.opacity *= comboboxpopups;

    }
    effects->paintWindow(w, mask, region, data);
}

bool TranslucencyEffect::isInactive(const EffectWindow* w) const
{
    if (active == w || w->isDock() || !w->isManaged())
        return false;
    if (NULL != active && NULL != active->group())
        if (active->group() == w->group())
            return false;
    if (!w->isNormalWindow() && !w->isDialog() && !w->isDock())
        return false;
    return true;
}

void TranslucencyEffect::slotWindowStartStopUserMovedResized(EffectWindow* w)
{
    if (m_activeMoveResize) {
        checkIsActive();
        moveresize_timeline.setCurrentTime(0);
        w->addRepaintFull();
    }
}

void TranslucencyEffect::slotWindowActivated(EffectWindow* w)
{
    if (inactive != 1.0) {
        activeinactive_timeline.setCurrentTime(0);
        if (NULL != active && active != w) {
            if ((NULL == w || w->group() != active->group()) &&
                    NULL != active->group()) {
                // Active group has changed. so repaint old group
                foreach (EffectWindow * tmp, active->group()->members())
                tmp->addRepaintFull();
            } else
                active->addRepaintFull();
        }

        if (NULL != w) {
            if (NULL != w->group()) {
                // Repaint windows in new group
                foreach (EffectWindow * tmp, w->group()->members())
                tmp->addRepaintFull();
            } else
                w->addRepaintFull();
        }
    }
    active = w;
    checkIsActive();
}

bool TranslucencyEffect::isActive() const
{
    return m_active;
}

} // namespace

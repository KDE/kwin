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

#ifndef KWIN_TRANSLUCENCY_H
#define KWIN_TRANSLUCENCY_H

#include <kwineffects.h>
#include <QtCore/QTimeLine>

namespace KWin
{

class TranslucencyEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal decoration READ configuredDecoration)
    Q_PROPERTY(qreal moveResize READ configuredMoveResize)
    Q_PROPERTY(qreal dialogs READ configuredDialogs)
    Q_PROPERTY(qreal inactive READ configuredInactive)
    Q_PROPERTY(qreal comboboxPopups READ configuredComboboxPopups)
    Q_PROPERTY(qreal menus READ configuredMenus)
    Q_PROPERTY(bool individualMenuConfig READ isIndividualMenuConfig)
    Q_PROPERTY(qreal dropDownMenus READ configuredDropDownMenus)
    Q_PROPERTY(qreal popupMenus READ configuredPopupMenus)
    Q_PROPERTY(qreal tornOffMenus READ configuredTornOffMenus)
    Q_PROPERTY(int moveResizeDuration READ configuredMoveResizeDuration)
    Q_PROPERTY(int activeInactiveDuration READ configuredActiveInactiveDuration)
public:
    TranslucencyEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);

    // for properties
    qreal configuredDecoration() const {
        return decoration;
    }
    qreal configuredMoveResize() const {
        return moveresize;
    }
    qreal configuredDialogs() const {
        return dialogs;
    }
    qreal configuredInactive() const {
        return inactive;
    }
    qreal configuredComboboxPopups() const {
        return comboboxpopups;
    }
    qreal configuredMenus() const {
        return menus;
    }
    bool isIndividualMenuConfig() const {
        return individualmenuconfig;
    }
    qreal configuredDropDownMenus() const {
        return dropdownmenus;
    }
    qreal configuredPopupMenus() const {
        return popupmenus;
    }
    qreal configuredTornOffMenus() const {
        return tornoffmenus;
    }
    int configuredMoveResizeDuration() const {
        return moveresize_timeline.duration();
    }
    int configuredActiveInactiveDuration() const {
        return activeinactive_timeline.duration();
    }
public Q_SLOTS:
    void slotWindowActivated(KWin::EffectWindow* w);
    void slotWindowStartStopUserMovedResized(KWin::EffectWindow *w);

private:
    bool isInactive(const EffectWindow *w) const;
    bool individualmenuconfig;

    double decoration;
    double moveresize;
    double dialogs;
    double inactive;
    double comboboxpopups;
    double menus;
    double dropdownmenus;
    double popupmenus;
    double tornoffmenus;

    EffectWindow* fadeout;
    EffectWindow* current;
    EffectWindow* previous;
    EffectWindow* active;

    QTimeLine moveresize_timeline;
    QTimeLine activeinactive_timeline;
};

} // namespace

#endif

/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef KWIN_FLIPSWITCH_H
#define KWIN_FLIPSWITCH_H

#include <kwineffects.h>
#include <KShortcut>
#include <QQueue>
#include <QTimeLine>

class KShortcut;

namespace KWin
{

class FlipSwitchEffect
    : public Effect
{
    Q_OBJECT
public:
    FlipSwitchEffect();
    ~FlipSwitchEffect();

    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool borderActivated(ElectricBorder border);
    virtual void grabbedKeyboardEvent(QKeyEvent* e);
    virtual bool isActive() const;

    static bool supported();
private Q_SLOTS:
    void toggleActiveCurrent();
    void toggleActiveAllDesktops();
    void globalShortcutChangedCurrent(QKeySequence shortcut);
    void globalShortcutChangedAll(QKeySequence shortcut);
    void slotWindowAdded(EffectWindow* w);
    void slotWindowClosed(EffectWindow *w);
    void slotTabBoxAdded(int mode);
    void slotTabBoxClosed();
    void slotTabBoxUpdated();

private:
    class ItemInfo;
    enum SwitchingDirection {
        DirectionForward,
        DirectionBackward
    };
    enum FlipSwitchMode {
        TabboxMode,
        CurrentDesktopMode,
        AllDesktopsMode
    };
    void setActive(bool activate, FlipSwitchMode mode);
    bool isSelectableWindow(EffectWindow *w) const;
    void scheduleAnimation(const SwitchingDirection& direction, int distance = 1);
    void adjustWindowMultiScreen(const EffectWindow *w, WindowPaintData& data);
    QQueue< SwitchingDirection> m_scheduledDirections;
    EffectWindow* m_selectedWindow;
    QTimeLine m_timeLine;
    QTimeLine m_startStopTimeLine;
    QTimeLine::CurveShape m_currentAnimationShape;
    QRect m_screenArea;
    int m_activeScreen;
    bool m_active;
    bool m_start;
    bool m_stop;
    bool m_animation;
    bool m_hasKeyboardGrab;
    Window m_input;
    FlipSwitchMode m_mode;
    EffectFrame* m_captionFrame;
    QFont m_captionFont;
    EffectWindowList m_flipOrderedWindows;
    QHash< const EffectWindow*, ItemInfo* > m_windows;
    // options
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_borderActivateAll;
    bool m_tabbox;
    bool m_tabboxAlternative;
    float m_angle;
    float m_xPosition;
    float m_yPosition;
    bool m_windowTitle;
    // Shortcuts
    KShortcut m_shortcutCurrent;
    KShortcut m_shortcutAll;
};

class FlipSwitchEffect::ItemInfo
{
public:
    ItemInfo();
    ~ItemInfo();
    bool deleted;
    double opacity;
    double brightness;
    double saturation;
};

} // namespace

#endif

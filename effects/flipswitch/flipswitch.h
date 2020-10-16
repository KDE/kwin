/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008, 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_FLIPSWITCH_H
#define KWIN_FLIPSWITCH_H

#include <kwineffects.h>
#include <QMatrix4x4>
#include <QQueue>
#include <QTimeLine>
#include <QFont>

namespace KWin
{

class FlipSwitchEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(bool tabBox READ isTabBox)
    Q_PROPERTY(bool tabBoxAlternative READ isTabBoxAlternative)
    Q_PROPERTY(int duration READ duration)
    Q_PROPERTY(int angle READ angle)
    Q_PROPERTY(qreal xPosition READ xPosition)
    Q_PROPERTY(qreal yPosition READ yPosition)
    Q_PROPERTY(bool windowTitle READ isWindowTitle)
public:
    FlipSwitchEffect();
    ~FlipSwitchEffect() override;

    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void grabbedKeyboardEvent(QKeyEvent* e) override;
    void windowInputMouseEvent(QEvent* e) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

    // for properties
    bool isTabBox() const {
        return m_tabbox;
    }
    bool isTabBoxAlternative() const {
        return m_tabboxAlternative;
    }
    int duration() const {
        return m_timeLine.duration();
    }
    int angle() const {
        return m_angle;
    }
    qreal xPosition() const {
        return m_xPosition;
    }
    qreal yPosition() const {
        return m_yPosition;
    }
    bool isWindowTitle() const {
        return m_windowTitle;
    }
private Q_SLOTS:
    void toggleActiveCurrent();
    void toggleActiveAllDesktops();
    void globalShortcutChanged(QAction *action, const QKeySequence &shortcut);
    void slotWindowAdded(KWin::EffectWindow* w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotTabBoxAdded(int mode);
    void slotTabBoxClosed();
    void slotTabBoxUpdated();
    void slotTabBoxKeyEvent(QKeyEvent* event);

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
    void selectNextOrPreviousWindow(bool forward);
    inline void selectNextWindow() { selectNextOrPreviousWindow(true); }
    inline void selectPreviousWindow() { selectNextOrPreviousWindow(false); }
    /**
     * Updates the caption of the caption frame.
     * Taking care of rewording the desktop client.
     * As well sets the icon for the caption frame.
     */
    void updateCaption();
    QQueue< SwitchingDirection> m_scheduledDirections;
    EffectWindow* m_selectedWindow;
    QTimeLine m_timeLine;
    QTimeLine m_startStopTimeLine;
    QEasingCurve m_currentAnimationEasingCurve;
    QRect m_screenArea;
    int m_activeScreen;
    bool m_active;
    bool m_start;
    bool m_stop;
    bool m_animation;
    bool m_hasKeyboardGrab;
    FlipSwitchMode m_mode;
    EffectFrame* m_captionFrame;
    QFont m_captionFont;
    EffectWindowList m_flipOrderedWindows;
    QHash< const EffectWindow*, ItemInfo* > m_windows;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_modelviewMatrix;
    // options
    bool m_tabbox;
    bool m_tabboxAlternative;
    float m_angle;
    float m_xPosition;
    float m_yPosition;
    bool m_windowTitle;
    // Shortcuts
    QList<QKeySequence> m_shortcutCurrent;
    QList<QKeySequence> m_shortcutAll;
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

/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_COVERSWITCH_H
#define KWIN_COVERSWITCH_H

#include <QHash>
#include <QRect>
#include <QRegion>
#include <QSize>
#include <QFont>
#include <QQueue>

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class CoverSwitchEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ configuredAnimationDuration)
    Q_PROPERTY(bool animateSwitch READ isAnimateSwitch)
    Q_PROPERTY(bool animateStart READ isAnimateStart)
    Q_PROPERTY(bool animateStop READ isAnimateStop)
    Q_PROPERTY(bool reflection READ isReflection)
    Q_PROPERTY(bool windowTitle READ isWindowTitle)
    Q_PROPERTY(qreal zPosition READ windowZPosition)
    Q_PROPERTY(bool primaryTabBox READ isPrimaryTabBox)
    Q_PROPERTY(bool secondaryTabBox READ isSecondaryTabBox)
    // TODO: mirror colors
public:
    CoverSwitchEffect();
    ~CoverSwitchEffect() override;

    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void windowInputMouseEvent(QEvent *e) override;
    bool isActive() const override;

    static bool supported();

    // for properties
    int configuredAnimationDuration() const {
        return animationDuration.count();
    }
    bool isAnimateSwitch() const {
        return animateSwitch;
    }
    bool isAnimateStart() const {
        return animateStart;
    }
    bool isAnimateStop() const {
        return animateStop;
    }
    bool isReflection() const {
        return reflection;
    }
    bool isWindowTitle() const {
        return windowTitle;
    }
    qreal windowZPosition() const {
        return zPosition;
    }
    bool isPrimaryTabBox() const {
        return primaryTabBox;
    }
    bool isSecondaryTabBox() const {
        return secondaryTabBox;
    }

    int requestedEffectChainPosition() const override {
        return 50;
    }

public Q_SLOTS:
    void slotWindowClosed(KWin::EffectWindow *c);
    void slotTabBoxAdded(int mode);
    void slotTabBoxClosed();
    void slotTabBoxUpdated();
    void slotTabBoxKeyEvent(QKeyEvent* event);

private:
    void paintScene(EffectWindow* frontWindow, const EffectWindowList& leftWindows, const EffectWindowList& rightWindows,
                    bool reflectedWindows = false);
    void paintWindowCover(EffectWindow* w, bool reflectedWindow, WindowPaintData& data);
    void paintFrontWindow(EffectWindow* frontWindow, int width, int leftWindows, int rightWindows, bool reflectedWindow);
    void paintWindows(const EffectWindowList& windows, bool left, bool reflectedWindows, EffectWindow* additionalWindow = nullptr);
    void selectNextOrPreviousWindow(bool forward);
    inline void selectNextWindow() { selectNextOrPreviousWindow(true); }
    inline void selectPreviousWindow() { selectNextOrPreviousWindow(false); }
    void abort();
    /**
     * Updates the caption of the caption frame.
     * Taking care of rewording the desktop client.
     * As well sets the icon for the caption frame.
     */
    void updateCaption();

    bool mActivated;
    float angle;
    bool animateSwitch;
    bool animateStart;
    bool animateStop;
    bool animation;
    bool start;
    bool stop;
    bool reflection;
    float mirrorColor[2][4];
    bool windowTitle;
    std::chrono::milliseconds animationDuration;
    bool stopRequested;
    bool startRequested;
    TimeLine timeLine;
    QRect area;
    float zPosition;
    float scaleFactor;
    enum Direction {
        Left,
        Right
    };
    Direction direction;
    QQueue<Direction> scheduled_directions;
    EffectWindow* selected_window;
    int activeScreen;
    QList< EffectWindow* > leftWindows;
    QList< EffectWindow* > rightWindows;
    EffectWindowList currentWindowList;
    EffectWindowList referrencedWindows;

    EffectFrame* captionFrame;
    QFont captionFont;

    bool primaryTabBox;
    bool secondaryTabBox;

    GLShader *m_reflectionShader;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_modelviewMatrix;
};

} // namespace

#endif

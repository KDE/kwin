/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_WOBBLYWINDOWS_H
#define KWIN_WOBBLYWINDOWS_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

struct ParameterSet;

/**
 * Effect which wobble windows
 */
class WobblyWindowsEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal stiffness READ stiffness)
    Q_PROPERTY(qreal drag READ drag)
    Q_PROPERTY(qreal moveFactor READ moveFactor)
    Q_PROPERTY(qreal xTesselation READ xTesselation)
    Q_PROPERTY(qreal yTesselation READ yTesselation)
    Q_PROPERTY(qreal minVelocity READ minVelocity)
    Q_PROPERTY(qreal maxVelocity READ maxVelocity)
    Q_PROPERTY(qreal stopVelocity READ stopVelocity)
    Q_PROPERTY(qreal minAcceleration READ minAcceleration)
    Q_PROPERTY(qreal maxAcceleration READ maxAcceleration)
    Q_PROPERTY(qreal stopAcceleration READ stopAcceleration)
    Q_PROPERTY(bool moveWobble READ isMoveWobble)
    Q_PROPERTY(bool resizeWobble READ isResizeWobble)
public:

    WobblyWindowsEffect();
    ~WobblyWindowsEffect() override;

    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        // Please notice that the Wobbly Windows effect has to be placed
        // after the Maximize effect in the effect chain, otherwise there
        // can be visual artifacts when dragging maximized windows.
        return 70;
    }

    // Wobbly model parameters
    void setStiffness(qreal stiffness);
    void setDrag(qreal drag);
    void setVelocityThreshold(qreal velocityThreshold);
    void setMoveFactor(qreal factor);

    struct Pair {
        qreal x;
        qreal y;
    };

    enum WindowStatus {
        Free,
        Moving,
    };

    static bool supported();

    // for properties
    qreal stiffness() const {
        return m_stiffness;
    }
    qreal drag() const {
        return m_drag;
    }
    qreal moveFactor() const {
        return m_move_factor;
    }
    qreal xTesselation() const {
        return m_xTesselation;
    }
    qreal yTesselation() const {
        return m_yTesselation;
    }
    qreal minVelocity() const {
        return m_minVelocity;
    }
    qreal maxVelocity() const {
        return m_maxVelocity;
    }
    qreal stopVelocity() const {
        return m_stopVelocity;
    }
    qreal minAcceleration() const {
        return m_minAcceleration;
    }
    qreal maxAcceleration() const {
        return m_maxAcceleration;
    }
    qreal stopAcceleration() const {
        return m_stopAcceleration;
    }
    bool isMoveWobble() const {
        return m_moveWobble;
    }
    bool isResizeWobble() const {
        return m_resizeWobble;
    }

public Q_SLOTS:
    void slotWindowStartUserMovedResized(KWin::EffectWindow *w);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w, const QRect &geometry);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);
    void slotWindowMaximizeStateChanged(KWin::EffectWindow *w, bool horizontal, bool vertical);

private:
    void startMovedResized(EffectWindow* w);
    void stepMovedResized(EffectWindow* w);
    bool updateWindowWobblyDatas(EffectWindow* w, qreal time);

    struct WindowWobblyInfos {
        Pair* origin;
        Pair* position;
        Pair* velocity;
        Pair* acceleration;
        Pair* buffer;

        // if true, the physics system moves this point based only on it "normal" destination
        // given by the window position, ignoring neighbour points.
        bool* constraint;

        unsigned int width;
        unsigned int height;
        unsigned int count;

        Pair* bezierSurface;
        unsigned int bezierWidth;
        unsigned int bezierHeight;
        unsigned int bezierCount;

        WindowStatus status;

        // for resizing. Only sides that have moved will wobble
        bool can_wobble_top, can_wobble_left, can_wobble_right, can_wobble_bottom;
        QRect resize_original_rect;
    };

    QHash< const EffectWindow*,  WindowWobblyInfos > windows;

    QRegion m_updateRegion;

    qreal m_stiffness;
    qreal m_drag;
    qreal m_move_factor;

    // the default tesselation for windows
    // use qreal instead of int as I really often need
    // these values as real to do divisions.
    qreal m_xTesselation;
    qreal m_yTesselation;

    qreal m_minVelocity;
    qreal m_maxVelocity;
    qreal m_stopVelocity;
    qreal m_minAcceleration;
    qreal m_maxAcceleration;
    qreal m_stopAcceleration;

    bool m_moveWobble;
    bool m_resizeWobble;

    void initWobblyInfo(WindowWobblyInfos& wwi, QRect geometry) const;
    void freeWobblyInfo(WindowWobblyInfos& wwi) const;

    WobblyWindowsEffect::Pair computeBezierPoint(const WindowWobblyInfos& wwi, Pair point) const;

    static void heightRingLinearMean(Pair** data_pointer, WindowWobblyInfos& wwi);

    void setParameterSet(const ParameterSet& pset);
};

} // namespace KWin

#endif // WOBBLYWINDOWS_H

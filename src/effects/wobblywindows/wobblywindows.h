/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Include with base class for effects.
#include <kwinoffscreeneffect.h>

namespace KWin
{

struct ParameterSet;

/**
 * Effect which wobble windows
 */
class WobblyWindowsEffect : public OffscreenEffect
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
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
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

    struct Pair
    {
        qreal x;
        qreal y;
    };

    enum WindowStatus {
        Free,
        Moving,
    };

    static bool supported();

    // for properties
    qreal stiffness() const;
    qreal drag() const;
    qreal moveFactor() const;
    qreal xTesselation() const;
    qreal yTesselation() const;
    qreal minVelocity() const;
    qreal maxVelocity() const;
    qreal stopVelocity() const;
    qreal minAcceleration() const;
    qreal maxAcceleration() const;
    qreal stopAcceleration() const;
    bool isMoveWobble() const;
    bool isResizeWobble() const;

protected:
    void apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads) override;

public Q_SLOTS:
    void slotWindowStartUserMovedResized(KWin::EffectWindow *w);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w, const QRectF &geometry);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);
    void slotWindowMaximizeStateChanged(KWin::EffectWindow *w, bool horizontal, bool vertical);

private:
    void startMovedResized(EffectWindow *w);
    void stepMovedResized(EffectWindow *w);
    bool updateWindowWobblyDatas(EffectWindow *w, qreal time);

    struct WindowWobblyInfos
    {
        QVector<Pair> origin;
        QVector<Pair> position;
        QVector<Pair> velocity;
        QVector<Pair> acceleration;
        QVector<Pair> buffer;

        // if true, the physics system moves this point based only on it "normal" destination
        // given by the window position, ignoring neighbour points.
        QVector<bool> constraint;

        unsigned int width;
        unsigned int height;
        unsigned int count;

        QVector<Pair> bezierSurface;
        unsigned int bezierWidth;
        unsigned int bezierHeight;
        unsigned int bezierCount;

        WindowStatus status;

        // for resizing. Only sides that have moved will wobble
        bool can_wobble_top, can_wobble_left, can_wobble_right, can_wobble_bottom;
        QRectF resize_original_rect;

        std::chrono::milliseconds clock;
    };

    QHash<const EffectWindow *, WindowWobblyInfos> windows;

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

    void initWobblyInfo(WindowWobblyInfos &wwi, QRectF geometry) const;

    WobblyWindowsEffect::Pair computeBezierPoint(const WindowWobblyInfos &wwi, Pair point) const;

    static void heightRingLinearMean(QVector<Pair> &data, WindowWobblyInfos &wwi);

    void setParameterSet(const ParameterSet &pset);
};

} // namespace KWin

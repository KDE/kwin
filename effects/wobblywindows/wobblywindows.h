/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Cédric Borgese <cedric.borgese@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_WOBBLYWINDOWS_H
#define KWIN_WOBBLYWINDOWS_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

struct ParameterSet;

/**
 * Effect which wobble windows
 **/
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
    Q_PROPERTY(bool moveEffectEnabled READ isMoveEffectEnabled)
    Q_PROPERTY(bool openEffectEnabled READ isOpenEffectEnabled)
    Q_PROPERTY(bool closeEffectEnabled READ isCloseEffectEnabled)
    Q_PROPERTY(bool moveWobble READ isMoveWobble)
    Q_PROPERTY(bool resizeWobble READ isResizeWobble)
public:

    WobblyWindowsEffect();
    virtual ~WobblyWindowsEffect();

    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintScreen();
    virtual bool isActive() const;

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
        Openning,
        Closing
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
    bool isMoveEffectEnabled() const {
        return m_moveEffectEnabled;
    }
    bool isOpenEffectEnabled() const {
        return m_openEffectEnabled;
    }
    bool isCloseEffectEnabled() const {
        return m_closeEffectEnabled;
    }
    bool isMoveWobble() const {
        return m_moveWobble;
    }
    bool isResizeWobble() const {
        return m_resizeWobble;
    }
public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);
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

        // for closing
        QRectF closeRect;

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

    bool m_moveEffectEnabled;
    bool m_openEffectEnabled;
    bool m_closeEffectEnabled;

    bool m_moveWobble; // Expands m_moveEffectEnabled
    bool m_resizeWobble;

    void initWobblyInfo(WindowWobblyInfos& wwi, QRect geometry) const;
    void freeWobblyInfo(WindowWobblyInfos& wwi) const;
    void wobblyOpenInit(WindowWobblyInfos& wwi) const;
    void wobblyCloseInit(WindowWobblyInfos& wwi, EffectWindow* w) const;

    WobblyWindowsEffect::Pair computeBezierPoint(const WindowWobblyInfos& wwi, Pair point) const;

    static void heightRingLinearMean(Pair** data_pointer, WindowWobblyInfos& wwi);

    void setParameterSet(const ParameterSet& pset);
};

} // namespace KWin

#endif // WOBBLYWINDOWS_H

/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

#include <chrono>

namespace KWin
{

/**
 * The SpringMotion class simulates the motion of a spring along one dimension using the
 * mass-spring-damper model. The sping constant parameter controls the acceleration of the
 * spring. The damping ratio controls the oscillation of the spring.
 */
class SpringMotion
{
public:
    SpringMotion();
    SpringMotion(qreal springConstant, qreal dampingRatio);

    /**
     * Advance the simulation by the given @a delta milliseconds.
     */
    void advance(std::chrono::milliseconds delta);
    bool isMoving() const;

    /**
     * Returns the current velocity.
     */
    qreal velocity() const;
    void setVelocity(qreal velocity);

    /**
     * Returns the current position.
     */
    qreal position() const;
    void setPosition(qreal position);

    /**
     * Returns the anchor position. It's the position that the spring is pulled towards.
     */
    qreal anchor() const;
    void setAnchor(qreal anchor);

    /**
     * Returns the spring constant. It controls the acceleration of the spring.
     */
    qreal springConstant() const;

    /**
     * Returns the damping ratio. It controls the oscillation of the spring. Potential values:
     *
     * - 0 or undamped: the spring will oscillate indefinitely
     * - less than 1 or underdamped: the mass tends to overshoot its starting position, but with
     *   every oscillation some energy is dissipated and the oscillation dies away
     * - 1 or critically damped: the mass will fail to overshoot and make a single oscillation
     * - greater than 1 or overdamped: the mass slowly returns to the anchor position without
     *   overshooting
     */
    qreal dampingRatio() const;

    /**
     * If the distance of the mass between two consecutive simulations is smaller than the epsilon
     * value, consider that the mass has stopped moving.
     */
    qreal epsilon() const;
    void setEpsilon(qreal epsilon);

private:
    struct State
    {
        qreal position;
        qreal velocity;
    };

    struct Slope
    {
        qreal dp;
        qreal dv;
    };

    State integrate(const State &state, qreal dt);
    Slope evaluate(const State &state, qreal dt, const Slope &slope);

    State m_prev;
    State m_next;
    qreal m_t;
    qreal m_timestep;

    qreal m_anchor;
    qreal m_springConstant;
    qreal m_dampingRatio;
    qreal m_dampingCoefficient;
    qreal m_epsilon;
};

} // namespace KWin

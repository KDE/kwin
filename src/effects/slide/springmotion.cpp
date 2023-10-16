/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "springmotion.h"

#include <cmath>

namespace KWin
{

static qreal lerp(qreal a, qreal b, qreal t)
{
    return a * (1 - t) + b * t;
}

SpringMotion::SpringMotion()
    : SpringMotion(200.0, 1.1)
{
}

SpringMotion::SpringMotion(qreal springConstant, qreal dampingRatio)
    : m_prev({0, 0})
    , m_next({0, 0})
    , m_t(1.0)
    , m_timestep(1.0 / 100.0)
    , m_anchor(0)
    , m_springConstant(springConstant)
    , m_dampingRatio(dampingRatio)
    , m_dampingCoefficient(2 * std::sqrt(m_springConstant) * m_dampingRatio)
    , m_epsilon(1.0)
{
}

bool SpringMotion::isMoving() const
{
    return std::fabs(position() - anchor()) > m_epsilon || std::fabs(velocity()) > m_epsilon;
}

qreal SpringMotion::springConstant() const
{
    return m_springConstant;
}

qreal SpringMotion::dampingRatio() const
{
    return m_dampingRatio;
}

qreal SpringMotion::velocity() const
{
    return lerp(m_prev.velocity, m_next.velocity, m_t);
}

void SpringMotion::setVelocity(qreal velocity)
{
    m_next = State{
        .position = position(),
        .velocity = velocity,
    };
    m_t = 1.0;
}

qreal SpringMotion::position() const
{
    return lerp(m_prev.position, m_next.position, m_t);
}

void SpringMotion::setPosition(qreal position)
{
    m_next = State{
        .position = position,
        .velocity = velocity(),
    };
    m_t = 1.0;
}

qreal SpringMotion::epsilon() const
{
    return m_epsilon;
}

void SpringMotion::setEpsilon(qreal epsilon)
{
    m_epsilon = epsilon;
}

qreal SpringMotion::anchor() const
{
    return m_anchor;
}

void SpringMotion::setAnchor(qreal anchor)
{
    m_anchor = anchor;
}

SpringMotion::Slope SpringMotion::evaluate(const State &state, qreal dt, const Slope &slope)
{
    const State next{
        .position = state.position + slope.dp * dt,
        .velocity = state.velocity + slope.dv * dt,
    };

    // The math here follows from the mass-spring-damper model equation.
    const qreal springForce = (m_anchor - next.position) * m_springConstant;
    const qreal dampingForce = -next.velocity * m_dampingCoefficient;
    const qreal acceleration = springForce + dampingForce;

    return Slope{
        .dp = state.velocity,
        .dv = acceleration,
    };
}

SpringMotion::State SpringMotion::integrate(const State &state, qreal dt)
{
    // Use Runge-Kutta method (RK4) to integrate the mass-spring-damper equation.
    const Slope initial{
        .dp = 0,
        .dv = 0,
    };
    const Slope k1 = evaluate(state, 0.0, initial);
    const Slope k2 = evaluate(state, 0.5 * dt, k1);
    const Slope k3 = evaluate(state, 0.5 * dt, k2);
    const Slope k4 = evaluate(state, dt, k3);

    const qreal dpdt = 1.0 / 6.0 * (k1.dp + 2 * k2.dp + 2 * k3.dp + k4.dp);
    const qreal dvdt = 1.0 / 6.0 * (k1.dv + 2 * k2.dv + 2 * k3.dv + k4.dv);

    return State{
        .position = state.position + dpdt * dt,
        .velocity = state.velocity + dvdt * dt,
    };
}

void SpringMotion::advance(std::chrono::milliseconds delta)
{
    if (!isMoving()) {
        return;
    }

    // If m_springConstant is infinite, we have an animation time factor of zero.
    // As such, we should advance to the target immediately.
    if (std::isinf(m_springConstant)) {
        m_next = State{
            .position = m_anchor,
            .velocity = 0.0,
        };
        return;
    }

    // If the delta interval is not multiple of m_timestep precisely, the previous and
    // the next samples will be linearly interpolated to get current position and velocity.
    const qreal steps = (delta.count() / 1000.0) / m_timestep;
    for (m_t += steps; m_t > 1.0; m_t -= 1.0) {
        m_prev = m_next;
        m_next = integrate(m_next, m_timestep);
    }
}

} // namespace KWin

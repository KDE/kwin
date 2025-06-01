/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// KWin
#include <effect/quickeffect.h>
#include <input_event_spy.h>

// Qt
#include <QAction>
#include <QTimer>
#include <QVariantAnimation>

// std
#include <chrono>
#include <memory>

namespace KWin
{

class StrokeEffect : public QuickSceneEffect, public InputEventSpy
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isStrokeActive NOTIFY strokeActiveChanged) // not to be mixed up with Effect::isActive()
    Q_PROPERTY(int animationDurationMsec READ animationDurationMsec NOTIFY animationDurationMsecChanged)

public:
    StrokeEffect();
    ~StrokeEffect() override;

    // Effect overrides
    void reconfigure(ReconfigureFlags) override;

    bool isStrokeActive() const;

    int animationDurationMsec() const;
    void setAnimationDurationMsec(int msec);

    // InputEventSpy overrides
    void strokeGestureBegin(Qt::KeyboardModifiers modifiers, const QPointF &initial, const QPointF &latest, std::chrono::microseconds time) override;
    void strokeGestureUpdate(const QPointF &latest, bool startingNewSegment, std::chrono::microseconds time) override;
    void strokeGestureEnd(std::chrono::microseconds time) override;
    void strokeGestureCancelled(std::chrono::microseconds time) override;

public Q_SLOTS:
    void activate();
    void deactivate(int timeout);

Q_SIGNALS:
    void strokeActiveChanged();
    void animationDurationMsecChanged();

    void strokeStarted(const QPointF &initial);
    void strokePointAdded(const QPointF &latest);
    void strokePointReplaced(const QPointF &latest);
    void strokeEnded();
    void strokeCancelled();

protected:
    QVariantMap initialProperties(Output *screen) override;

private:
    void realDeactivate();

private:
    // configuration
    std::vector<std::unique_ptr<QAction>> m_actions;
    int m_animationDurationMsec = 200;

    // visualization
    std::unique_ptr<QTimer> m_shutdownTimer;
    bool m_isStrokeActive = false;
};

} // namespace KWin

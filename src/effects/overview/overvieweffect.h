/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinquickeffect.h>

namespace KWin
{

class OverviewEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(int layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool ignoreMinimized READ ignoreMinimized NOTIFY ignoreMinimizedChanged)
    Q_PROPERTY(bool blurBackground READ blurBackground NOTIFY blurBackgroundChanged)
    Q_PROPERTY(qreal partialActivationFactor READ partialActivationFactor NOTIFY partialActivationFactorChanged)
    // More efficient from a property binding pov rather than binding to partialActivationFactor !== 0
    Q_PROPERTY(bool gestureInProgress READ gestureInProgress NOTIFY gestureInProgressChanged)
    Q_PROPERTY(QString searchText MEMBER m_searchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(QPointF desktopOffset READ desktopOffset NOTIFY desktopOffsetChanged)

public:
    enum class Status {
        Inactive,
        Activating,
        Deactivating,
        Active
    };
    OverviewEffect();
    ~OverviewEffect() override;

    int layout() const;
    void setLayout(int layout);

    bool ignoreMinimized() const;

    int animationDuration() const;
    void setAnimationDuration(int duration);

    bool blurBackground() const;
    void setBlurBackground(bool blur);

    qreal partialActivationFactor() const;
    void setPartialActivationFactor(qreal factor);

    bool gestureInProgress() const;
    void setGestureInProgress(bool gesture);

    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;
    QPointF desktopOffset();

    QString state() const;
    void setState(QString state);

Q_SIGNALS:
    void animationDurationChanged();
    void layoutChanged();
    void blurBackgroundChanged();
    void partialActivationFactorChanged();
    void gestureInProgressChanged();
    void ignoreMinimizedChanged();
    void searchTextChanged();
    void stateChanged();
    void desktopOffsetChanged();

public Q_SLOTS:
    void activate();
    void partialActivate(qreal factor);
    void deactivate();
    void quickDeactivate();
    void toggle();

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

private:
    void realDeactivate();

    QTimer *m_shutdownTimer;
    QAction *m_toggleAction = nullptr;
    QAction *m_realtimeToggleAction = nullptr;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_touchBorderActivate;
    QString m_searchText;
    Status m_status = Status::Inactive;
    qreal m_partialActivationFactor = 0;
    bool m_blurBackground = false;
    int m_animationDuration = 400;
    int m_layout = 1;
    bool m_gestureInProgress = false;
    QString m_state = "initial";
    QPointF m_desktopOffset = QPointF(0, 0);
};

} // namespace KWin

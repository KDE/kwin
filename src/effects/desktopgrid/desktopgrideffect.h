/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinquickeffect.h>

namespace KWin
{

class DesktopGridEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int gridRows READ gridRows NOTIFY gridRowsChanged)
    Q_PROPERTY(int gridColumns READ gridColumns NOTIFY gridColumnsChanged)
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(int layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(qreal partialActivationFactor READ partialActivationFactor NOTIFY partialActivationFactorChanged)
    Q_PROPERTY(bool gestureInProgress READ gestureInProgress NOTIFY gestureInProgressChanged)
    Q_PROPERTY(bool showAddRemove READ showAddRemove NOTIFY showAddRemoveChanged)
    Q_PROPERTY(Qt::AlignmentFlag desktopNameAlignment READ desktopNameAlignment NOTIFY desktopNameAlignmentChanged)
    Q_PROPERTY(DesktopLayoutMode desktopLayoutMode READ desktopLayoutMode NOTIFY desktopLayoutModeChanged)
    Q_PROPERTY(int customLayoutRows READ customLayoutRows NOTIFY customLayoutRowsChanged)

public:
    enum class DesktopLayoutMode {
        LayoutPager,
        LayoutAutomatic,
        LayoutCustom
    };
    Q_ENUM(DesktopLayoutMode)

    enum class Status {
        Inactive,
        Activating,
        Deactivating,
        Active
    };

    DesktopGridEffect();
    ~DesktopGridEffect() override;

    int layout() const;
    void setLayout(int layout);

    int animationDuration() const;
    void setAnimationDuration(int duration);

    bool showAddRemove() const;

    qreal partialActivationFactor() const;
    void setPartialActivationFactor(qreal factor);

    bool gestureInProgress() const;
    void setGestureInProgress(bool gesture);

    int gridRows() const;
    int gridColumns() const;

    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

    Qt::AlignmentFlag desktopNameAlignment() const;
    DesktopLayoutMode desktopLayoutMode() const;
    int customLayoutRows() const;

    Q_INVOKABLE void addDesktop() const;
    Q_INVOKABLE void removeDesktop() const;
    Q_INVOKABLE void swapDesktops(int from, int to);

public Q_SLOTS:
    void activate();
    void partialActivate(qreal factor);
    void cancelPartialActivate();
    void partialDeactivate(qreal factor);
    void cancelPartialDeactivate();
    void deactivate(int timeout);
    void toggle();

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

Q_SIGNALS:
    void gridRowsChanged();
    void gridColumnsChanged();
    void animationDurationChanged();
    void layoutChanged();
    void partialActivationFactorChanged();
    void gestureInProgressChanged();
    void showAddRemoveChanged();
    void desktopNameAlignmentChanged();
    void desktopLayoutModeChanged();
    void customLayoutRowsChanged();

private:
    void realDeactivate();

    QTimer *m_shutdownTimer;
    qreal m_partialActivationFactor = 0;
    QAction *m_toggleAction = nullptr;
    QAction *m_realtimeToggleAction = nullptr;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_touchBorderActivate;
    Status m_status = Status::Inactive;
    int m_animationDuration = 400;
    int m_layout = 1;
    bool m_gestureInProgress = false;
};

} // namespace KWin

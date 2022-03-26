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
    Q_PROPERTY(int layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool ignoreMinimized READ ignoreMinimized NOTIFY ignoreMinimizedChanged)
    Q_PROPERTY(bool blurBackground READ blurBackground NOTIFY blurBackgroundChanged)

public:
    OverviewEffect();
    ~OverviewEffect() override;

    int layout() const;
    void setLayout(int layout);

    bool ignoreMinimized() const;

    bool blurBackground() const;
    void setBlurBackground(bool blur);

    int requestedEffectChainPosition() const override;
    bool borderActivated(ElectricBorder border) override;
    void reconfigure(ReconfigureFlags flags) override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

Q_SIGNALS:
    void animationDurationChanged();
    void layoutChanged();
    void blurBackgroundChanged();
    void partialActivationFactorChanged();
    void gestureInProgressChanged();
    void ignoreMinimizedChanged();

public Q_SLOTS:
    void toggle();

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

private:

    QAction *m_toggleAction = nullptr;
    QList<QKeySequence> m_toggleShortcut;
    QList<ElectricBorder> m_borderActivate;
    qreal p_partialActivationFactor = 0;
    bool m_blurBackground = false;
    int m_layout = 1;
    bool m_gestureInProgress = false;
};

} // namespace KWin

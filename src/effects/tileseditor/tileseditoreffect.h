/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinquickeffect.h>

namespace KWin
{

class TilesEditorEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)

public:
    TilesEditorEffect();
    ~TilesEditorEffect() override;

    void reconfigure(ReconfigureFlags) override;

    int animationDuration() const;
    void setAnimationDuration(int duration);

    int requestedEffectChainPosition() const override;
    void grabbedKeyboardEvent(QKeyEvent *keyEvent) override;

public Q_SLOTS:
    void toggle();
    void activate();
    void deactivate(int timeout);

Q_SIGNALS:
    void animationDurationChanged();

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

private:
    void realDeactivate();

    std::unique_ptr<QTimer> m_shutdownTimer;
    std::unique_ptr<QAction> m_toggleAction;
    QList<QKeySequence> m_toggleShortcut;
    int m_animationDuration = 200;
};

} // namespace KWin

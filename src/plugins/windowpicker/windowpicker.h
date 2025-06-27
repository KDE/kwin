/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/quickeffect.h"

#include <QFuture>
#include <QPromise>

namespace KWin
{

class Window;
class CrossFadeEffect;

class WindowPicker : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(bool livePreviews MEMBER m_livePreviews NOTIFY livePreviewsChanged)

public:
    WindowPicker();
    ~WindowPicker() override;

    int animationDuration() const;
    void setAnimationDuration(int duration);

    void reconfigure(ReconfigureFlags) override;
    int requestedEffectChainPosition() const override;

public Q_SLOTS:
    QFuture<EffectWindow *> pickWindow(bool livePreviews);
    void windowPicked(KWin::Window *window);

Q_SIGNALS:
    void animationDurationChanged();
    void livePreviewsChanged();

private:
    void realDeactivate();

    QTimer *m_shutdownTimer;
    QString m_searchText;
    int m_animationDuration;
    bool m_livePreviews;
    QPromise<EffectWindow *> m_promise;
};

} // namespace KWin

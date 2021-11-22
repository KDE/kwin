/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinquickeffect.h>

namespace KWin
{

class WindowViewEffect : public QuickSceneEffect
{
    Q_OBJECT

public:
    WindowViewEffect();
    ~WindowViewEffect() override;

    int requestedEffectChainPosition() const override;

public Q_SLOTS:
    void activate(const QStringList &windowIds);
    void deactivate(int timeout);

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

private:
    void realDeactivate();

    QTimer *m_shutdownTimer;
    QList<QUuid> m_windowIds;
};

} // namespace KWin

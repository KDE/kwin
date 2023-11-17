/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "effect/effect.h"

#include <QTimer>

#include <unordered_map>

namespace KWin
{
class OffscreenQuickScene;

class OutputLocatorEffect : public KWin::Effect
{
    Q_OBJECT

public:
    explicit OutputLocatorEffect(QObject *parent = nullptr);
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen) override;
    bool isActive() const override;

public Q_SLOTS:
    void show();
    void hide();

private:
    QUrl m_qmlUrl;
    QTimer m_showTimer;
    std::unordered_map<Output *, std::unique_ptr<OffscreenQuickScene>> m_scenesByScreens;
};
}

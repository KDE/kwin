/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <kwineffects.h>

#include <QTimer>
#include <QWindow>

namespace KWin
{
class OffscreenQuickScene;

class OutputLocatorEffect : public KWin::Effect
{
    Q_OBJECT

public:
    explicit OutputLocatorEffect(QObject *parent = nullptr);
    void paintScreen(int mask, const QRegion &region, KWin::ScreenPaintData &data) override;
    bool isActive() const override;

public Q_SLOTS:
    void show();
    void hide();

private:
    QUrl m_qmlUrl;
    QTimer m_showTimer;
    std::unique_ptr<QWindow> m_dummyWindow;
    QMap<EffectScreen *, OffscreenQuickScene *> m_scenesByScreens;
};
}

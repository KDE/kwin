/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <KWindowSystem/private/kwindoweffects_p.h>
#include <QObject>
#include <kwindowsystem_version.h>

namespace KWin
{

class WindowEffects : public QObject, public KWindowEffectsPrivate
{
public:
    WindowEffects();
    ~WindowEffects() override;

    bool isEffectAvailable(KWindowEffects::Effect effect) override;
    void slideWindow(WId id, KWindowEffects::SlideFromLocation location, int offset) override;
    void enableBlurBehind(WId window, bool enable = true, const QRegion &region = QRegion()) override;
    void enableBackgroundContrast(WId window, bool enable = true, qreal contrast = 1, qreal intensity = 1, qreal saturation = 1, const QRegion &region = QRegion()) override;
};

}

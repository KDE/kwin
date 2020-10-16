/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <KWindowSystem/private/kwindoweffects_p.h>
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
#if KWINDOWSYSTEM_VERSION <= QT_VERSION_CHECK(5, 61, 0)
    void slideWindow(QWidget *widget, KWindowEffects::SlideFromLocation location) override;
#endif
    QList<QSize> windowSizes(const QList<WId> &ids) override;
    void presentWindows(WId controller, const QList<WId> &ids) override;
    void presentWindows(WId controller, int desktop = NET::OnAllDesktops) override;
    void highlightWindows(WId controller, const QList<WId> &ids) override;
    void enableBlurBehind(WId window, bool enable = true, const QRegion &region = QRegion()) override;
    void enableBackgroundContrast(WId window, bool enable = true, qreal contrast = 1, qreal intensity = 1, qreal saturation = 1, const QRegion &region = QRegion()) override;
#if KWINDOWSYSTEM_BUILD_DEPRECATED_SINCE(5, 67)
    void markAsDashboard(WId window) override;
#endif
};

}

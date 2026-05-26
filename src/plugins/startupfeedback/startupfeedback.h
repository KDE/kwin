/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "effect/effect.h"
#include "effect/timeline.h"
#include <KConfigWatcher>

#if KWIN_BUILD_X11
#include <KStartupInfo>
#endif
#include <QIcon>
#include <QObject>

#include <chrono>

namespace KWin
{

class ImageItem;

class StartupFeedbackEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int type READ type)

public:
    StartupFeedbackEffect();
    ~StartupFeedbackEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 90;
    }

    int type() const
    {
        return int(m_type);
    }

    static bool supported();

private Q_SLOTS:
    void gotNewStartup(const QString &id, const QIcon &icon);
    void gotRemoveStartup(const QString &id);
    void gotStartupChange(const QString &id, const QIcon &icon);

private:
    enum FeedbackType {
        NoFeedback,
        BouncingFeedback,
        BlinkingFeedback,
        PassiveFeedback,
    };

    struct Startup
    {
        QIcon icon;
        std::shared_ptr<QTimer> expiredTimer;
    };

    void start(const Startup &startup);
    void stop();
    QPoint feedbackOffset() const;
    QSize feedbackIconSize() const;

    bool cursorEffectivelyHidden() const;
    void maybeStartAfterCursorChange();

    qreal m_bounceSizesRatio;
#if KWIN_BUILD_X11
    KStartupInfo *m_startupInfo;
#endif
    QString m_currentStartup;
    QMap<QString, Startup> m_startups;
    bool m_active;
    int m_frame;
    int m_progress;
    AnimationClock m_clock;
    std::unique_ptr<ImageItem> m_item;
    std::unique_ptr<ImageItem> m_blinkItem;
    FeedbackType m_type;
    int m_cursorSize;
    KConfigWatcher::Ptr m_configWatcher;
    bool m_splashVisible;
    std::chrono::seconds m_timeout;
};

} // namespace

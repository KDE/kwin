/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_STARTUPFEEDBACK_H
#define KWIN_STARTUPFEEDBACK_H
#include <QObject>
#include <kwineffects.h>
#include <KStartupInfo>
#include <KConfigWatcher>

class KSelectionOwner;
namespace KWin
{
class GLTexture;

class StartupFeedbackEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int type READ type)
public:
    StartupFeedbackEffect();
    ~StartupFeedbackEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 90;
    }

    int type() const {
        return int(m_type);
    }

    static bool supported();

private Q_SLOTS:
    void gotNewStartup(const KStartupInfoId& id, const KStartupInfoData& data);
    void gotRemoveStartup(const KStartupInfoId& id, const KStartupInfoData& data);
    void gotStartupChange(const KStartupInfoId& id, const KStartupInfoData& data);
    void slotMouseChanged(const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

private:
    enum FeedbackType {
        NoFeedback,
        BouncingFeedback,
        BlinkingFeedback,
        PassiveFeedback
    };
    void start(const QString& icon);
    void stop();
    QImage scalePixmap(const QPixmap& pm, const QSize& size) const;
    void prepareTextures(const QPixmap& pix);
    QRect feedbackRect() const;

    qreal m_bounceSizesRatio;
    KStartupInfo* m_startupInfo;
    KSelectionOwner* m_selection;
    KStartupInfoId m_currentStartup;
    QMap< KStartupInfoId, QString > m_startups; // QString == pixmap
    bool m_active;
    int m_frame;
    int m_progress;
    GLTexture* m_bouncingTextures[5];
    GLTexture* m_texture; // for passive and blinking
    FeedbackType m_type;
    QRect m_currentGeometry, m_dirtyRect;
    GLShader *m_blinkingShader;
    int m_cursorSize;
    KConfigWatcher::Ptr m_configWatcher;
};
} // namespace

#endif
